/*
 * EnergyLink.cpp
 *
 *  Created on: Apr 19, 2015
 *      Author: rlcevg
 */

#include "resource/EnergyLink.h"
#include "resource/MetalManager.h"
#include "module/EconomyManager.h"  // Only for MexDef
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

void CEnergyLink::Update()
{
	int lastFrame = circuit->GetLastFrame();
	if (markFrame + FRAMES_PER_SEC >= lastFrame) {
		return;
	}
	markFrame = lastFrame;

	circuit->UpdateFriendlyUnits();
	CCircuitDef* mexDef = circuit->GetEconomyManager()->GetMexDef();
	std::list<CCircuitUnit*> mexes, pylons;
	const CAllyTeam::Units& friendlies = circuit->GetFriendlyUnits();
	for (auto& kv : friendlies) {
		CCircuitUnit* unit = kv.second;
		if (*unit->GetCircuitDef() == *mexDef) {
			mexes.push_back(unit);
		} else if (pylonRanges.find(unit->GetCircuitDef()->GetId()) != pylonRanges.end()) {
			pylons.push_back(unit);
		}
	}

	MarkAllyMexes(mexes);
	RebuildTree();

	MarkAllyPylons(pylons);
	CheckLink();

//	boost::breadth_first_search();
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

	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();
	// FIXME: No-metal-spots maps can crash: check !links.empty()
	links.resize(boost::num_edges(clusterGraph));
	linkIt = boost::make_iterator_property_map(&links[0], boost::get(&CMetalData::Edge::index, clusterGraph));

	spanningGraph = CMetalData::Graph(boost::num_vertices(clusterGraph));

	for (int i = 0; i < metalManager->GetClusters().size(); ++i) {
		linkedClusters[i] = {0, false};
	}

	// FIXME: DEBUG
//	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>([this]() {
//		// Clear Kruskal drawing
//		const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
//		for (int i = 0; i < clusters.size(); ++i) {
//			circuit->GetDrawer()->DeletePointsAndLines(clusters[i].geoCentr);
//		}
//		const CMetalData::Graph& clusterGraph = circuit->GetMetalManager()->GetGraph();
//		CMetalData::Graph::edge_iterator edgeIt, edgeEnd;
//		std::tie(edgeIt, edgeEnd) = boost::edges(clusterGraph);
//		for (; edgeIt != edgeEnd; ++edgeIt) {
//			Link &link = boost::get(linkIt, *edgeIt);
//			for (auto& kv: link.pylons) {
//				circuit->GetDrawer()->DeletePointsAndLines(markedPylons[kv.first].pos);
//			}
//		}
//		Update();
//	}), FRAMES_PER_SEC * 30);
//	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>([this]() {
//		// Draw Kruskal
//		const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
//		const CMetalData::Graph& clusterGraph = circuit->GetMetalManager()->GetGraph();
////		for (auto& edge : spanningTree) {
////			const AIFloat3& posFrom = clusters[boost::source(edge, clusterGraph)].geoCentr;
////			const AIFloat3& posTo = clusters[boost::target(edge, clusterGraph)].geoCentr;
////			circuit->GetDrawer()->AddLine(posFrom, posTo);
////		}
//
//		CMetalData::Graph::edge_iterator edgeIt, edgeEnd;
//		std::tie(edgeIt, edgeEnd) = boost::edges(clusterGraph);
//		for (; edgeIt != edgeEnd; ++edgeIt) {
//			Link &link = boost::get(linkIt, *edgeIt);
//			if (link.pylons.empty()) {
//				continue;
//			}
//
//			std::vector<std::pair<Structure*, float>> buildings;
//			buildings.reserve(link.pylons.size());
//			for (auto& kv: link.pylons) {
//				buildings.push_back(std::make_pair(&markedPylons[kv.first], kv.second));
//			}
//			// Sorted by distance from graph vertex to unit
//			auto compare = [](const std::pair<Structure*, float>& lhs, const std::pair<Structure*, float>& rhs) {
//				return lhs.second < rhs.second;
//			};
//			std::sort(buildings.begin(), buildings.end(), compare);
//
//			AIFloat3 start = buildings.front().first->pos;
//			for (auto& kv : buildings) {
//				circuit->GetDrawer()->AddLine(start, kv.first->pos);
//				start = kv.first->pos;
//			}
//		}
//	}), FRAMES_PER_SEC * 30, FRAMES_PER_SEC * 3);
	// FIXME: DEBUG
}

void CEnergyLink::MarkAllyPylons(const std::list<CCircuitUnit*>& pylons)
{
	Structures newUnits, oldUnits;
	for (auto unit : pylons) {
		CCircuitUnit::Id unitId = unit->GetId();
		decltype(markedPylons)::iterator it = markedPylons.find(unitId);
		if (it == markedPylons.end()) {
			Structure building;
			building.cdefId = unit->GetCircuitDef()->GetId();
			building.pos = unit->GetUnit()->GetPos();

			newUnits[unitId] = building;
			AddPylon(unitId, building);
		} else {
			oldUnits.insert(*it);
		}
	}

	auto cmp = [](const Structures::value_type& lhs, const Structures::value_type& rhs) {
		return lhs.first < rhs.first;
	};

	Structures deadUnits;
	std::set_difference(markedPylons.begin(), markedPylons.end(),
						oldUnits.begin(), oldUnits.end(),
						std::inserter(deadUnits, deadUnits.end()), cmp);
	for (auto& kv : deadUnits) {
		RemovePylon(kv.first, kv.second);
	}
	markedPylons.clear();
	std::set_union(oldUnits.begin(), oldUnits.end(),
				   newUnits.begin(), newUnits.end(),
				   std::inserter(markedPylons, markedPylons.end()), cmp);
}

void CEnergyLink::AddPylon(CCircuitUnit::Id unitId, const Structure& building)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(building.pos);
	if (index < 0) {
		return;
	}
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	const AIFloat3& P = building.pos;
	const AIFloat3& P0 = clusters[index].geoCentr;
	float sqRange = pylonRanges[building.cdefId];
	sqRange *= sqRange;
	auto isBoundsValid = [&P0, &P, sqRange](const AIFloat3& P1) {
		float dx = P1.x - P0.x;
		float dz = P1.z - P0.z;
		float mult = ((P.x - P0.x) * dx + (P.z - P0.z) * dz) / (dx * dx + dz * dz);

		float projX = P0.x + dx * mult;
		float projZ = P0.z + dz * mult;
		return ((std::min(P0.x, P1.x) <= projX) && (projX <= std::max(P0.x, P1.x)) &&
				(std::min(P0.z, P1.z) <= projZ) && (projZ <= std::max(P0.z, P1.z)));
	};
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
	std::list<CMetalData::Graph::out_edge_iterator> linkEdgeItsVR;  // Edges which vertex is in pylon-range
	std::tie(edgeIt, edgeEnd) = boost::out_edges(index, clusterGraph);  // or boost::tie
	float sqMinDist = std::numeric_limits<float>::max();
	for (; edgeIt != edgeEnd; ++edgeIt) {
		int idxTarget = boost::target(*edgeIt, clusterGraph);
		const AIFloat3& P1 = clusters[idxTarget].geoCentr;

		if ((P0.SqDistance2D(P) < sqRange) || (P1.SqDistance2D(P) < sqRange)) {
			linkEdgeItsVR.push_back(edgeIt);
			continue;
		}

		if (!isBoundsValid(P1)) {
			continue;
		}

		float sqDist = sqDistPointToLine(P1);
		if (std::fabs(sqDist - sqMinDist) < EPSILON) {
			linkEdgeIts.push_back(edgeIt);
		} else if (sqDist < sqMinDist) {
			linkEdgeIts.clear();
			linkEdgeIts.push_back(edgeIt);
			sqMinDist = sqDist;
		}
	}
	linkEdgeIts.splice(linkEdgeIts.end(), linkEdgeItsVR);

	for (auto& edgeIt : linkEdgeIts) {
		const CMetalData::EdgeDesc& edgeId = *edgeIt;
		Link& link = boost::get(linkIt, edgeId);
		CMetalData::VertexDesc vertId = boost::source(edgeId, clusterGraph);
		link.pylons[unitId] = clusters[vertId].geoCentr.distance2D(building.pos);
//		linkPylons.insert(clusterGraph[edgeId].index);
		linkPylons.insert(edgeId);
	}
}

void CEnergyLink::RemovePylon(CCircuitUnit::Id unitId, const Structure& building)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(building.pos);
	if (index < 0) {
		return;
	}
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	// Find edges to which building belongs to: linkEdgeIts
	CMetalData::Graph::out_edge_iterator edgeIt, edgeEnd;
	std::list<CMetalData::Graph::out_edge_iterator> linkEdgeIts;
	std::tie(edgeIt, edgeEnd) = boost::out_edges(index, clusterGraph);  // or boost::tie
	float sqMinDist = std::numeric_limits<float>::max();
	for (; edgeIt != edgeEnd; ++edgeIt) {
		const CMetalData::EdgeDesc& edgeId = *edgeIt;
		Link& link = boost::get(linkIt, edgeId);
		if ((link.pylons.erase(unitId) > 0)) {
			unlinkPylons.insert(edgeId);
		}
	}
}

void CEnergyLink::CheckLink()
{
	if (linkPylons.empty() && unlinkPylons.empty()) {
		return;
	}
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	for (auto edgeId : linkPylons) {
		Link& link = boost::get(linkIt, edgeId);
		if (link.isFinished || !link.isValid) {
			continue;
		}
		// TODO : Use sorted container at AddPylon
		std::vector<std::pair<Structure*, float>> buildings;
		buildings.reserve(link.pylons.size());
		for (auto& kv : link.pylons) {
			buildings.push_back(std::make_pair(&markedPylons[kv.first], kv.second));
		}
		// Sorted by distance from graph vertex to unit
		auto compare = [](const std::pair<Structure*, float>& lhs, const std::pair<Structure*, float>& rhs) {
			return lhs.second < rhs.second;
		};
		std::sort(buildings.begin(), buildings.end(), compare);

		decltype(buildings)::value_type& pair0 = buildings.front();
		if (pair0.second > pylonRanges[pair0.first->cdefId]) {
			break;
		}
		const AIFloat3& p1 = clusters[boost::target(edgeId, clusterGraph)].geoCentr;
		decltype(buildings)::value_type& pair1 = buildings.back();
		if (p1.distance2D(pair1.first->pos) > pylonRanges[pair1.first->cdefId]) {
			break;
		}
		bool valid = true;
		for (int i = 1; i < buildings.size(); ++i) {
			Structure* b0 = buildings[i - 1].first;
			Structure* b1 = buildings[i].first;
			float sumRange = pylonRanges[b0->cdefId] + pylonRanges[b1->cdefId];
			if (b0->pos.SqDistance2D(b1->pos) > sumRange * sumRange) {
				valid = false;
				break;
			}
		}
		link.isFinished = valid;
	}
	linkPylons.clear();

	for (auto edgeId : unlinkPylons) {
		Link& link = boost::get(linkIt, edgeId);
		if (!link.isFinished || !link.isValid) {
			continue;
		}
		// TODO : Use sorted container at AddPylon
		std::vector<std::pair<Structure*, float>> buildings;
		buildings.reserve(link.pylons.size());
		for (auto& kv : link.pylons) {
			buildings.push_back(std::make_pair(&markedPylons[kv.first], kv.second));
		}
		// Sorted by distance from graph vertex to unit
		auto compare = [](const std::pair<Structure*, float>& lhs, const std::pair<Structure*, float>& rhs) {
			return lhs.second < rhs.second;
		};
		std::sort(buildings.begin(), buildings.end(), compare);

		decltype(buildings)::value_type& pair0 = buildings.front();
		if (pair0.second > pylonRanges[pair0.first->cdefId]) {
			link.isFinished = false;
			break;
		}
		const AIFloat3& p1 = clusters[boost::target(edgeId, clusterGraph)].geoCentr;
		decltype(buildings)::value_type& pair1 = buildings.back();
		if (p1.distance2D(pair1.first->pos) > pylonRanges[pair1.first->cdefId]) {
			link.isFinished = false;
			break;
		}
		bool valid = true;
		for (int i = 1; i < buildings.size(); ++i) {
			Structure* b0 = buildings[i - 1].first;
			Structure* b1 = buildings[i].first;
			float sumRange = pylonRanges[b0->cdefId] + pylonRanges[b1->cdefId];
			if (b0->pos.SqDistance2D(b1->pos) > sumRange * sumRange) {
				valid = false;
				break;
			}
		}
		link.isFinished = valid;
	}
	unlinkPylons.clear();
}

void CEnergyLink::MarkAllyMexes(const std::list<CCircuitUnit*>& mexes)
{
	Structures newUnits, oldUnits;
	for (auto unit : mexes) {
		CCircuitUnit::Id unitId = unit->GetId();
		decltype(markedMexes)::iterator it = markedMexes.find(unitId);
		if (it == markedMexes.end()) {
			Structure building;
			building.pos = unit->GetUnit()->GetPos();

			newUnits[unitId] = building;
			AddMex(building.pos);
		} else {
			oldUnits.insert(*it);
		}
	}

	auto cmp = [](const Structures::value_type& lhs, const Structures::value_type& rhs) {
		return lhs.first < rhs.first;
	};

	decltype(markedMexes) deadUnits;
	std::set_difference(markedMexes.begin(), markedMexes.end(),
						oldUnits.begin(), oldUnits.end(),
						std::inserter(deadUnits, deadUnits.end()), cmp);
	for (auto& kv : deadUnits) {
		RemoveMex(kv.second.pos);
	}
	markedMexes.clear();
	std::set_union(oldUnits.begin(), oldUnits.end(),
				   newUnits.begin(), newUnits.end(),
				   std::inserter(markedMexes, markedMexes.end()), cmp);
}

void CEnergyLink::AddMex(const AIFloat3& pos)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(pos);
	LinkVertex& lv = linkedClusters[index];
	if ((index < 0) || (++lv.mexCount < metalManager->GetClusters()[index].idxSpots.size())) {
		return;
	}

	linkClusters.push_back(index);
	lv.isConnected = true;
}

void CEnergyLink::RemoveMex(const AIFloat3& pos)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(pos);
	LinkVertex& lv = linkedClusters[index];
	if ((index < 0) || (lv.mexCount-- < metalManager->GetClusters()[index].idxSpots.size())) {
		return;
	}

	unlinkClusters.push_back(index);
	lv.isConnected = false;
}

void CEnergyLink::RebuildTree()
{
	if (linkClusters.empty() && unlinkClusters.empty()) {
		return;
	}
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();

	// Mark used edges as const
	for (auto& edgeId : spanningTree) {
		if (boost::get(linkIt, edgeId).isBeingBuilt) {
			spanningGraph[edgeId].weight = EPSILON;
		}
	}

	// Add new edges to Kruskal graph
	for (auto index : linkClusters) {
		CMetalData::Graph::out_edge_iterator outEdgeIt, outEdgeEnd;
		std::tie(outEdgeIt, outEdgeEnd) = boost::out_edges(index, clusterGraph);  // or boost::tie
		for (; outEdgeIt != outEdgeEnd; ++outEdgeIt) {
			const CMetalData::EdgeDesc& edgeId = *outEdgeIt;
			int idx1 = boost::target(edgeId, clusterGraph);
			if (linkedClusters[idx1].isConnected) {
				boost::add_edge(index, idx1, clusterGraph[edgeId], spanningGraph);
			}
		}
	}
	linkClusters.clear();

	// Remove destroyed edges
	auto pred = [](const CMetalData::EdgeDesc& desc) {
		return true;
	};
	for (auto index : unlinkClusters) {
		boost::remove_out_edge_if(index, pred, spanningGraph);
	}
	unlinkClusters.clear();

	// Build Kruskal's minimum spanning tree
	spanningTree.clear();
	boost::property_map<CMetalData::Graph, float CMetalData::Edge::*>::type w_map = boost::get(&CMetalData::Edge::weight, spanningGraph);
	boost::kruskal_minimum_spanning_tree(spanningGraph, std::back_inserter(spanningTree), boost::weight_map(w_map));
}

} // namespace circuit
