/*
 * EnergyGrid.cpp
 *
 *  Created on: Apr 19, 2015
 *      Author: rlcevg
 */

#include "resource/EnergyGrid.h"
#include "resource/MetalManager.h"
#include "module/EconomyManager.h"  // Only for MexDef
#include "setup/SetupManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include <boost/graph/kruskal_min_spanning_tree.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <algorithm>
#include <exception>

namespace circuit {

using namespace springai;

class CExitBFS: public std::exception {
public:
	CExitBFS(CEnergyLink* link) : std::exception(), link(link) {}
	virtual const char* what() const throw() {
		return "BFS goal has been reached";
	}
	CEnergyLink* link;
};

class detect_link : public boost::bfs_visitor<> {
public:
	detect_link(const CEnergyGrid::link_iterator_t& it) : linkIt(it) {}
    template <class Edge, class Graph>
	void tree_edge(Edge e, const Graph& g) {
		CEnergyLink& link = boost::get(linkIt, e);
		if (!link.IsFinished() && !link.IsBeingBuilt() && link.IsValid()) {
			throw CExitBFS(&link);
		}
	}
    CEnergyGrid::link_iterator_t linkIt;
};

struct spanning_tree {
	spanning_tree() {}
	spanning_tree(const std::set<CMetalData::EdgeDesc>& edges) : spanningTree(edges) {}
	template <typename Edge>
	bool operator()(const Edge& e) const {
		return (spanningTree.find(e) != spanningTree.end());
	}
	std::set<CMetalData::EdgeDesc> spanningTree;
};

CEnergyGrid::CEnergyGrid(CCircuitAI* circuit) :
		circuit(circuit),
		markFrame(-1),
		isForceRebuild(false)
{
	circuit->GetScheduler()->RunParallelTask(std::make_shared<CGameTask>(&CEnergyGrid::Init, this));
}

CEnergyGrid::~CEnergyGrid()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CEnergyGrid::Update()
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
	CheckGrid();
}

CEnergyLink* CEnergyGrid::GetLinkToBuild()
{
	spanning_tree filter(spanningTree);
	boost::filtered_graph<CMetalData::Graph, spanning_tree> fg(ownedClusters, filter);
	detect_link vis(linkIt);
	const AIFloat3& pos = circuit->GetSetupManager()->GetStartPos();
	int index = circuit->GetMetalManager()->FindNearestCluster(pos);
	if (index < 0) {
		return nullptr;
	}

	CEnergyLink* link = nullptr;
	try {
		boost::breadth_first_search(fg, boost::vertex(index, ownedClusters), boost::visitor(vis));
	} catch (const CExitBFS& e) {
		link = e.link;
	}
	return link;
}

void CEnergyGrid::Init()
{
	for (auto& kv : circuit->GetCircuitDefs()) {
		const std::map<std::string, std::string>& customParams = kv.second->GetUnitDef()->GetCustomParams();
		auto it = customParams.find("pylonrange");
		if (it != customParams.end()) {
			pylonRanges[kv.first] = utils::string_to_float(it->second);
		}
	}

	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();
	// FIXME: No-metal-spots maps can crash: check !links.empty()
	links.reserve(boost::num_edges(clusterGraph));
	CMetalData::Graph::edge_iterator edgeIt, edgeEnd;
	std::tie(edgeIt, edgeEnd) = boost::edges(clusterGraph);
	for (; edgeIt != edgeEnd; ++edgeIt) {
		const CMetalData::EdgeDesc& edgeId = *edgeIt;
		const AIFloat3& startPos = clusters[boost::source(edgeId, clusterGraph)].geoCentr;
		const AIFloat3& endPos = clusters[boost::target(edgeId, clusterGraph)].geoCentr;
		links.push_back(CEnergyLink(startPos, endPos));
	}
	linkIt = boost::make_iterator_property_map(&links[0], boost::get(&CMetalData::Edge::index, clusterGraph));

	ownedClusters = CMetalData::Graph(boost::num_vertices(clusterGraph));

	linkedClusters.resize(metalManager->GetClusters().size(), {0, false});

	// FIXME: DEBUG
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>([this]() {
		// Clear Kruskal drawing
		const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
		for (int i = 0; i < clusters.size(); ++i) {
			circuit->GetDrawer()->DeletePointsAndLines(clusters[i].geoCentr);
		}
//		const CMetalData::Graph& clusterGraph = circuit->GetMetalManager()->GetGraph();
//		CMetalData::Graph::edge_iterator edgeIt, edgeEnd;
//		std::tie(edgeIt, edgeEnd) = boost::edges(clusterGraph);
//		for (; edgeIt != edgeEnd; ++edgeIt) {
//			CEnergyLink &link = boost::get(linkIt, *edgeIt);
//			for (auto& kv: link.pylons) {
//				circuit->GetDrawer()->DeletePointsAndLines(markedPylons[kv.first]);
//			}
//		}
		Update();
	}), FRAMES_PER_SEC * 30);
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>([this]() {
		// Draw Kruskal
		const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
		const CMetalData::Graph& clusterGraph = circuit->GetMetalManager()->GetGraph();
		for (const CMetalData::EdgeDesc& edge : spanningTree) {
			const AIFloat3& posFrom = clusters[boost::source(edge, clusterGraph)].geoCentr;
			const AIFloat3& posTo = clusters[boost::target(edge, clusterGraph)].geoCentr;
			circuit->GetDrawer()->AddLine(posFrom, posTo);
		}

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
	}), FRAMES_PER_SEC * 30, FRAMES_PER_SEC * 3);
	// FIXME: DEBUG
}

void CEnergyGrid::MarkAllyPylons(const std::list<CCircuitUnit*>& pylons)
{
	Structures newUnits, oldUnits;
	for (CCircuitUnit* unit : pylons) {
		CCircuitUnit::Id unitId = unit->GetId();
		Structures::iterator it = markedPylons.find(unitId);
		if (it == markedPylons.end()) {
			const AIFloat3& pos = unit->GetUnit()->GetPos();
			newUnits[unitId] = pos;
			AddPylon(unit, pos);
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

void CEnergyGrid::AddPylon(CCircuitUnit* unit, const AIFloat3& P)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(P);
	if (index < 0) {
		return;
	}
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();
	const AIFloat3& P0 = clusters[index].geoCentr;

	// Find edges to which building belongs to: linkEdgeIts
	CMetalData::Graph::out_edge_iterator edgeIt, edgeEnd;
	std::tie(edgeIt, edgeEnd) = boost::out_edges(index, clusterGraph);  // or boost::tie
	float sqMinDist = std::numeric_limits<float>::max();
	float range = pylonRanges[unit->GetCircuitDef()->GetId()];
	float sqRange = range * range;
	for (; edgeIt != edgeEnd; ++edgeIt) {
		const CMetalData::EdgeDesc& edgeId = *edgeIt;
		int idxTarget = boost::target(edgeId, clusterGraph);
		const AIFloat3& P1 = clusters[idxTarget].geoCentr;

		if ((P0.SqDistance2D(P) < sqRange) || (P1.SqDistance2D(P) < sqRange) ||
			(((P0 + P1) * 0.5f).SqDistance2D(P) < P0.SqDistance2D(P1) * 0.25f))
		{
			CEnergyLink& link = boost::get(linkIt, edgeId);
			link.AddPylon(unit->GetId(), P, range);
			linkPylons.insert(edgeId);
		}
	}
}

void CEnergyGrid::RemovePylon(CCircuitUnit::Id unitId, const AIFloat3& pos)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(pos);
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
		CEnergyLink& link = boost::get(linkIt, edgeId);
		if ((link.RemovePylon(unitId) > 0)) {
			unlinkPylons.insert(edgeId);
		}
	}
}

void CEnergyGrid::CheckGrid()
{
	if (linkPylons.empty() && unlinkPylons.empty()) {
		return;
	}

	for (const CMetalData::EdgeDesc& edgeId : linkPylons) {
		CEnergyLink& link = boost::get(linkIt, edgeId);
		if (link.IsFinished() || !link.IsValid()) {
			continue;
		}
		link.CheckConnection();
	}
	linkPylons.clear();

	for (const CMetalData::EdgeDesc& edgeId : unlinkPylons) {
		CEnergyLink& link = boost::get(linkIt, edgeId);
		if (!link.IsFinished() || !link.IsValid()) {
			continue;
		}
		link.CheckConnection();
	}
	unlinkPylons.clear();
}

void CEnergyGrid::MarkAllyMexes(const std::list<CCircuitUnit*>& mexes)
{
	Structures newUnits, oldUnits;
	for (CCircuitUnit* unit : mexes) {
		CCircuitUnit::Id unitId = unit->GetId();
		Structures::iterator it = markedMexes.find(unitId);
		if (it == markedMexes.end()) {
			const AIFloat3& pos = unit->GetUnit()->GetPos();
			newUnits[unitId] = pos;
			AddMex(pos);
		} else {
			oldUnits.insert(*it);
		}
	}

	auto cmp = [](const Structures::value_type& lhs, const Structures::value_type& rhs) {
		return lhs.first < rhs.first;
	};

	Structures deadUnits;
	std::set_difference(markedMexes.begin(), markedMexes.end(),
						oldUnits.begin(), oldUnits.end(),
						std::inserter(deadUnits, deadUnits.end()), cmp);
	for (auto& kv : deadUnits) {
		RemoveMex(kv.second);
	}
	markedMexes.clear();
	std::set_union(oldUnits.begin(), oldUnits.end(),
				   newUnits.begin(), newUnits.end(),
				   std::inserter(markedMexes, markedMexes.end()), cmp);
}

void CEnergyGrid::AddMex(const AIFloat3& pos)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(pos);
	if (index < 0) {
		return;
	}
	SLinkVertex& lv = linkedClusters[index];
	if (++lv.mexCount < metalManager->GetClusters()[index].idxSpots.size()) {
		return;
	}

	// FIXME: DEBUG
//	circuit->GetDrawer()->DeletePointsAndLines(pos);
//	circuit->GetDrawer()->AddPoint(pos, utils::int_to_string(temp++).c_str());
//	circuit->GetScheduler()->RunTaskAfter(std::make_shared<CGameTask>([this, pos, index]() {
//		circuit->GetDrawer()->DeletePointsAndLines(metalManager->GetClusters()[index].geoCentr);
//		circuit->GetDrawer()->AddPoint(metalManager->GetClusters()[index].geoCentr, "Full");
//	}), FRAMES_PER_SEC * 5);
	// FIXME: DEBUG

	linkClusters.push_back(index);
	lv.isConnected = true;
}

void CEnergyGrid::RemoveMex(const AIFloat3& pos)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(pos);
	if (index < 0) {
		return;
	}
	SLinkVertex& lv = linkedClusters[index];
	if (lv.mexCount-- < metalManager->GetClusters()[index].idxSpots.size()) {
		return;
	}

	// FIXME: DEBUG
//	circuit->GetDrawer()->DeletePointsAndLines(pos);
//	circuit->GetDrawer()->DeletePointsAndLines(circuit->GetMetalManager()->GetClusters()[index].geoCentr);
//	circuit->GetScheduler()->RunTaskAfter(std::make_shared<CGameTask>([this, pos, index]() {
//		circuit->GetDrawer()->AddPoint(pos, utils::int_to_string(temp++).c_str());
//		circuit->GetDrawer()->AddPoint(circuit->GetMetalManager()->GetClusters()[index].geoCentr, "Empty");
//	}), FRAMES_PER_SEC * 5);
	// FIXME: DEBUG

	unlinkClusters.push_back(index);
	lv.isConnected = false;
}

void CEnergyGrid::RebuildTree()
{
	if (linkClusters.empty() && unlinkClusters.empty() && !isForceRebuild) {
		return;
	}
	isForceRebuild = false;
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();

	// Add new edges to Kruskal graph
	for (int index : linkClusters) {
		CMetalData::Graph::out_edge_iterator outEdgeIt, outEdgeEnd;
		std::tie(outEdgeIt, outEdgeEnd) = boost::out_edges(index, clusterGraph);  // or boost::tie
		for (; outEdgeIt != outEdgeEnd; ++outEdgeIt) {
			const CMetalData::EdgeDesc& edgeId = *outEdgeIt;
			int idx0 = boost::target(edgeId, clusterGraph);
			if (linkedClusters[idx0].isConnected) {
				boost::add_edge(idx0, index, clusterGraph[edgeId], ownedClusters);

				CEnergyLink& link = boost::get(linkIt, edgeId);
				link.SetVertices(clusters[idx0].geoCentr, clusters[index].geoCentr);  // Set edge direction
			}
		}
	}
	linkClusters.clear();

	// Remove destroyed edges
	auto pred = [](const CMetalData::EdgeDesc& desc) {
		return true;
	};
	for (int index : unlinkClusters) {
		boost::remove_out_edge_if(index, pred, ownedClusters);
	}
	unlinkClusters.clear();

	// Mark used edges as const
	for (const CMetalData::EdgeDesc& edgeId : spanningTree) {
		CEnergyLink& link = boost::get(linkIt, edgeId);
		if (link.IsFinished() || link.IsBeingBuilt()) {
			ownedClusters[edgeId].weight = clusterGraph[edgeId].weight * 0.01f;
		} else if (!link.IsValid()) {
			ownedClusters[edgeId].weight = clusterGraph[edgeId].weight * 100.0f;
		}
	}

	// Build Kruskal's minimum spanning tree
	spanningTree.clear();
	boost::property_map<CMetalData::Graph, float CMetalData::Edge::*>::type w_map = boost::get(&CMetalData::Edge::weight, ownedClusters);
	boost::kruskal_minimum_spanning_tree(ownedClusters, std::inserter(spanningTree, spanningTree.end()), boost::weight_map(w_map));
}

} // namespace circuit
