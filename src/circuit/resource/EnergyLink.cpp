/*
 * EnergyLink.cpp
 *
 *  Created on: Apr 19, 2015
 *      Author: rlcevg
 */

#include "resource/EnergyLink.h"
#include "resource/MetalManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include <boost/graph/kruskal_min_spanning_tree.hpp>
#include <algorithm>

#define EPSILON 1.0f

namespace circuit {

using namespace springai;

CEnergyLink::CEnergyLink(CCircuitAI* circuit) :
		circuit(circuit),
		markFrame(-1)
{
	circuit->GetScheduler()->RunParallelTask(std::make_shared<CGameTask>(&CEnergyLink::Init, this));
}

CEnergyLink::~CEnergyLink()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CEnergyLink::LinkCluster(int index)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();

	// Mark previous edges as const
	CMetalData::Graph::edge_iterator edgeIt, edgeEnd;
	std::tie(edgeIt, edgeEnd) = boost::edges(spanningGraph);
	for (; edgeIt != edgeEnd; ++edgeIt) {
		spanningGraph[*edgeIt].weight = EPSILON;
	}

	// Add new edges to Kruskal graph
	CMetalData::Graph::out_edge_iterator outEdgeIt, outEdgeEnd;
	std::tie(outEdgeIt, outEdgeEnd) = boost::out_edges(index, clusterGraph);  // or boost::tie
	std::list<CMetalData::EdgeDesc> edgeAddon;
	for (; outEdgeIt != outEdgeEnd; ++outEdgeIt) {
		CMetalData::Graph::out_edge_iterator edIt, edEnd;
		std::tie(edIt, edEnd) = boost::out_edges(boost::target(*outEdgeIt, clusterGraph), spanningGraph);
		if (edIt != edEnd) {
			int idx0 = boost::source(*outEdgeIt, clusterGraph);
			int idx1 = boost::target(*outEdgeIt, clusterGraph);

			CMetalData::EdgeDesc edgeId;
			bool ok;
			std::tie(edgeId, ok) = boost::add_edge(idx0, idx1, spanningGraph);
			if (ok) {
				CMetalData::Edge& edge = spanningGraph[edgeId];
				edge.index = clusterGraph[*outEdgeIt].index;
				edge.weight = clusters[idx0].geoCentr.distance(clusters[idx1].geoCentr);
				edgeAddon.push_back(edgeId);
			}
		}
	}
	if (spanningTree.empty()) {
		boost::add_edge(index, index, spanningGraph);
	}

	if (circuit->GetAllyTeamId() == 0) {  // DEBUG
		// Clear Kruskal drawing
		for (auto& edge : spanningTree) {
			circuit->GetDrawer()->DeletePointsAndLines(clusters[(std::size_t)boost::source(edge, clusterGraph)].geoCentr);
		}
	}

	// Build Kruskal's minimum spanning tree
	spanningTree.clear();
	boost::property_map<CMetalData::Graph, float CMetalData::Edge::*>::type w_map = boost::get(&CMetalData::Edge::weight, spanningGraph);
	boost::kruskal_minimum_spanning_tree(spanningGraph, std::back_inserter(spanningTree), boost::weight_map(w_map));

	// Remove unused edges
	for (auto& edge : edgeAddon) {
		if (std::find(spanningTree.begin(), spanningTree.end(), edge) == spanningTree.end()) {
			boost::remove_edge(edge, spanningGraph);
		}
	}

	if (circuit->GetAllyTeamId() == 0) {  // DEBUG
		// Draw Kruskal
		for (auto& edge : spanningTree) {
			const AIFloat3& posFrom = clusters[boost::source(edge, clusterGraph)].geoCentr;
			const AIFloat3& posTo = clusters[boost::target(edge, clusterGraph)].geoCentr;
			circuit->GetDrawer()->AddLine(posFrom, posTo);
		}
	}
}

void CEnergyLink::Update()
{
	circuit->UpdateFriendlyUnits();

	MarkAllyPylons();
}

void CEnergyLink::Init()
{
	for (auto& kv : circuit->GetCircuitDefs()) {
		const std::map<std::string, std::string>& customParams = kv.second->GetUnitDef()->GetCustomParams();
		auto it = customParams.find("pylonrange");
		if (it != customParams.end()) {
			pylonRanges[kv.first] = utils::string_to_float(it->second);
		}
	}

	const CMetalData::Graph& clusterGraph = circuit->GetMetalManager()->GetGraph();
	// FIXME: No-metal-spots maps can crash: check !links.empty()
	links.resize(boost::num_edges(clusterGraph));
	linkIt = boost::make_iterator_property_map(&links[0], boost::get(&CMetalData::Edge::index, clusterGraph));

	spanningGraph = CMetalData::Graph(boost::num_vertices(clusterGraph));
}

void CEnergyLink::MarkAllyPylons()
{
	int lastFrame = circuit->GetLastFrame();
	if (markFrame + FRAMES_PER_SEC >= lastFrame) {
		return;
	}
	markFrame = lastFrame;

	circuit->UpdateFriendlyUnits();
	const CAllyTeam::Units& friendlies = circuit->GetFriendlyUnits();

	Structures newUnits, oldUnits;
	for (auto& kv : friendlies) {
		CCircuitUnit* unit = kv.second;
		Unit* u = unit->GetUnit();
		if (u->GetMaxSpeed() <= 0) {
			CCircuitUnit::Id unitId = kv.first;
			decltype(markedAllies)::iterator it = markedAllies.find(unitId);
			if (it == markedAllies.end()) {
				Structure building;
				building.cdefId = unit->GetCircuitDef()->GetId();
				building.pos = u->GetPos();

				newUnits[unitId] = building;
				MarkPylon(unitId, building, true);
			} else {
				oldUnits.insert(*it);
			}
		}
	}

	auto cmp = [](const Structures::value_type& lhs, const Structures::value_type& rhs) {
		return lhs.first < rhs.first;
	};

	Structures deadUnits;
	std::set_difference(markedAllies.begin(), markedAllies.end(),
						oldUnits.begin(), oldUnits.end(),
						std::inserter(deadUnits, deadUnits.end()), cmp);
	for (auto& kv : deadUnits) {
		MarkPylon(kv.first, kv.second, false);
	}
	markedAllies.clear();
	std::set_union(oldUnits.begin(), oldUnits.end(),
				   newUnits.begin(), newUnits.end(),
				   std::inserter(markedAllies, markedAllies.end()), cmp);
}

void CEnergyLink::MarkPylon(CCircuitUnit::Id unitId, const Structure& building, bool alive)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(building.pos);
	if (index < 0) {
		return;
	}
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	if (alive) {
		const AIFloat3& P = building.pos;
		const AIFloat3& P0 = clusters[index].geoCentr;
		auto sqDistPointToLine = [&P0, &P](const AIFloat3& P1) {
			float A = P0.z - P1.z;
			float B = P1.x - P0.x;
			float C = P0.x * P1.z - P1.x * P0.z;
			float denominator = A * P.x + B * P.z + C;
			float numerator = A * A + B * B;
			return denominator * denominator / numerator;
		};

		// Find edges to which building belongs to: linkEdgeIts
		CMetalData::Graph::out_edge_iterator edgeIt, edgeEnd;
		std::list<CMetalData::Graph::out_edge_iterator> linkEdgeIts;
		std::tie(edgeIt, edgeEnd) = boost::out_edges(index, clusterGraph);  // or boost::tie
		float sqMinDist = std::numeric_limits<float>::max();
		for (; edgeIt != edgeEnd; ++edgeIt) {
			int idxTarget = boost::target(*edgeIt, clusterGraph);
			float sqDist = sqDistPointToLine(clusters[idxTarget].geoCentr);
			if (std::fabs(sqDist - sqMinDist) < EPSILON) {
				linkEdgeIts.push_back(edgeIt);
			} else if (sqDist < sqMinDist) {
				linkEdgeIts.clear();
				linkEdgeIts.push_back(edgeIt);
				sqMinDist = sqDist;
			}
		}

		for (auto& edgeIt : linkEdgeIts) {
			Link& link = boost::get(linkIt, *edgeIt);
			link.pylons.insert(unitId);
//			if (link.isBeingBuilt) {
//
//			}
		}

	} else {

		for (auto& link : links) {
			if ((link.pylons.erase(unitId) > 0)) {
				// link.isBeingBuilt
			}
		}
	}

//	boost::breadth_first_search();

//	CMetalData::Edge& edge = clusterGraph[*linkEdgeIt];
//	// On insert
//	edge.insert();
//	if (edge.beingBuilt) {
//		// check if its finished!
//	}
//	// On erase
//	edge.erase();
//	edge.beingBuilt = numPylons > 0;
//	if (edge.beingBuilt) {
//		// check if its finished!
//	}
}

} // namespace circuit
