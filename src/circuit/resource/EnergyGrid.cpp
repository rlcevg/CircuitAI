/*
 * EnergyGrid.cpp
 *
 *  Created on: Apr 19, 2015
 *      Author: rlcevg
 */

#include "resource/EnergyGrid.h"
#include "resource/MetalManager.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Figure.h"

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
		if (!link.IsFinished() && link.IsValid()) {
			throw CExitBFS(&link);
		}
	}
    CEnergyGrid::link_iterator_t linkIt;
};

struct spanning_tree {
	spanning_tree() {}
	spanning_tree(const std::set<CMetalData::EdgeDesc>& edges, const CEnergyGrid::link_iterator_t& it) :
		spanningTree(edges),
		linkIt(it)
	{}
	template <typename Edge>
	bool operator()(const Edge& e) const {
		// NOTE: check for link.IsBeingBuilt solves vertex's pylon duplicates, but slows down grid construction
		return (spanningTree.find(e) != spanningTree.end()) && !boost::get(linkIt, e).IsBeingBuilt();
	}
	std::set<CMetalData::EdgeDesc> spanningTree;
    CEnergyGrid::link_iterator_t linkIt;
};

CEnergyGrid::CEnergyGrid(CCircuitAI* circuit)
		: circuit(circuit)
		, markFrame(-1)
		, isForceRebuild(false)
#ifdef DEBUG_VIS
		, figureFinishedId(-1)
		, figureBuildId(-1)
		, figureInvalidId(-1)
		, figureGridId(-1)
		, figureKruskalId(-1)
		, isVis(false)
		, toggleFrame(-1)
#endif
{
	circuit->GetScheduler()->RunParallelTask(std::make_shared<CGameTask>(&CEnergyGrid::Init, this));
}

CEnergyGrid::~CEnergyGrid()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CEnergyGrid::Update()
{
	if (markFrame /*+ FRAMES_PER_SEC*/ >= circuit->GetLastFrame()) {
		return;
	}
	markFrame = circuit->GetLastFrame();

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

	circuit->GetMetalManager()->MarkAllyMexes(mexes);
	MarkClusters();
	RebuildTree();

	MarkAllyPylons(pylons);
	CheckGrid();
}

CEnergyLink* CEnergyGrid::GetLinkToBuild(CCircuitDef*& outDef, AIFloat3& outPos)
{
	/*
	 * Detect link to build
	 */
	spanning_tree filter(spanningTree, linkIt);
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
	if (link == nullptr) {
		return nullptr;
	}

	/*
	 * Find best build def and position
	 */
	float searchRadius = circuit->GetEconomyManager()->GetPylonRange() * 0.5f;

	CEnergyLink::SVertex* v0 = link->GetV0();
	CEnergyLink::SVertex* v1 = link->GetV1();
	CEnergyLink::SPylon* pylon0 = link->GetConnectionHead(v0, v1->pos);
	if (pylon0 == nullptr) {
		outDef = circuit->GetEconomyManager()->GetPylonDef();  // IsAvailable check should be done before entering GetLinkToBuild
		outPos = circuit->GetTerrainManager()->FindBuildSite(outDef, v0->pos, searchRadius, UNIT_COMMAND_BUILD_NO_FACING);
		return link;
	}
	CEnergyLink::SPylon* pylon1 = link->GetConnectionHead(v1, pylon0->pos);
	if (pylon1 == nullptr) {
		outDef = circuit->GetEconomyManager()->GetPylonDef();  // IsAvailable check should be done before entering GetLinkToBuild
		outPos = circuit->GetTerrainManager()->FindBuildSite(outDef, v1->pos, searchRadius, UNIT_COMMAND_BUILD_NO_FACING);
		return link;
	}

	decltype(rangePylons) candDefs = rangePylons;
	while (!candDefs.empty()) {
		CCircuitDef::Id defId;
		float range;
		auto it = candDefs.rbegin();
		for (; it != candDefs.rend(); ++it) {
			defId = it->second;
			range = it->first;
			float dist = pylon0->range + pylon1->range + range;
			if (pylon0->pos.SqDistance2D(pylon1->pos) > dist * dist) {
				break;
			}
		}

		outDef = circuit->GetCircuitDef(defId);
		if (!outDef->IsAvailable()) {
			outPos = -RgtVector;
			candDefs.erase(range);
			continue;
		}

		AIFloat3 dir = pylon1->pos - pylon0->pos;
		AIFloat3 sweetPos;
		float dist = pylon0->range + pylon1->range + range * 2.0f;
		if (dir.SqLength2D() < dist * dist) {
			sweetPos = (pylon0->pos + dir.Normalize2D() * (pylon0->range - pylon1->range) + pylon1->pos) * 0.5f;
		} else {
			sweetPos = pylon0->pos + dir.Normalize2D() * (pylon0->range + range) * 0.95f;
		}

		outPos = circuit->GetTerrainManager()->FindBuildSite(outDef, sweetPos, searchRadius, UNIT_COMMAND_BUILD_NO_FACING);
		if (outPos == -RgtVector) {
//			candDefs.erase(std::next(it).base());  // http://stackoverflow.com/questions/1830158/how-to-call-erase-with-a-reverse-iterator
			candDefs.erase(range);
		} else {
			break;
		}
	}

	return link;
}

float CEnergyGrid::GetPylonRange(CCircuitDef::Id defId)
{
	auto it = pylonRanges.find(defId);
	return (it != pylonRanges.end()) ? it->second : .0f;
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
	// FIXME: const names
	const char* names[] = {"armestor", "armsolar", "armwin"};
	for (const char* name : names) {
		CCircuitDef* cdef = circuit->GetCircuitDef(name);
		rangePylons[pylonRanges[cdef->GetId()]] = cdef->GetId();
	}

	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	linkedClusters.resize(clusters.size(), false);

	// FIXME: No-metal-spots maps can crash: check !links.empty()
	links.reserve(boost::num_edges(clusterGraph));
	CMetalData::Graph::edge_iterator edgeIt, edgeEnd;
	std::tie(edgeIt, edgeEnd) = boost::edges(clusterGraph);
	for (; edgeIt != edgeEnd; ++edgeIt) {
		const CMetalData::EdgeDesc& edgeId = *edgeIt;
		int idx0 = boost::source(edgeId, clusterGraph);
		int idx1 = boost::target(edgeId, clusterGraph);
		links.emplace_back(idx0, clusters[idx0].geoCentr, idx1, clusters[idx1].geoCentr);
	}
	linkIt = boost::make_iterator_property_map(&links[0], boost::get(&CMetalData::SEdge::index, clusterGraph));

	ownedClusters = CMetalData::Graph(boost::num_vertices(clusterGraph));
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
			AddPylon(unitId, unit->GetCircuitDef()->GetId(), pos);
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
		RemovePylon(kv.first);
	}
	markedPylons.clear();
	std::set_union(oldUnits.begin(), oldUnits.end(),
				   newUnits.begin(), newUnits.end(),
				   std::inserter(markedPylons, markedPylons.end()), cmp);
}

void CEnergyGrid::AddPylon(CCircuitUnit::Id unitId, CCircuitDef::Id defId, const AIFloat3& pos)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	// Find edges to which building belongs to
	float range = pylonRanges[defId];
	float sqRange = range * range;
	CMetalData::Graph::edge_iterator edgeIt, edgeEnd;
	std::tie(edgeIt, edgeEnd) = boost::edges(clusterGraph);  // or boost::tie
	for (; edgeIt != edgeEnd; ++edgeIt) {
		const CMetalData::EdgeDesc& edgeId = *edgeIt;
		int idxSource = boost::source(edgeId, clusterGraph);
		const AIFloat3& P0 = clusters[idxSource].geoCentr;
		int idxTarget = boost::target(edgeId, clusterGraph);
		const AIFloat3& P1 = clusters[idxTarget].geoCentr;

		if ((P0.SqDistance2D(pos) < sqRange) || (P1.SqDistance2D(pos) < sqRange) ||
			(((P0 + P1) * 0.5f).SqDistance2D(pos) < P0.SqDistance2D(P1) * 0.25f))
		{
			CEnergyLink& link = boost::get(linkIt, edgeId);
			link.AddPylon(unitId, pos, range);
			linkPylons.insert(edgeId);
		}
	}
}

void CEnergyGrid::RemovePylon(CCircuitUnit::Id unitId)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	// Find edges to which building belongs to
	CMetalData::Graph::edge_iterator edgeIt, edgeEnd;
	std::tie(edgeIt, edgeEnd) = boost::edges(clusterGraph);  // or boost::tie
	for (; edgeIt != edgeEnd; ++edgeIt) {
		const CMetalData::EdgeDesc& edgeId = *edgeIt;
		CEnergyLink& link = boost::get(linkIt, edgeId);
		if (link.RemovePylon(unitId)) {
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

void CEnergyGrid::MarkClusters()
{
	const int size = circuit->GetMetalManager()->GetClusters().size();
	for (int index = 0; index < size; ++index) {
		bool isOur = circuit->GetMetalManager()->IsClusterOur(index);
		if (linkedClusters[index] == isOur) {
			continue;
		}
		(isOur ? linkClusters : unlinkClusters).push_back(index);
		linkedClusters[index] = isOur;
	}
}

void CEnergyGrid::RebuildTree()
{
	if (linkClusters.empty() && unlinkClusters.empty() && !isForceRebuild) {
		return;
	}
	isForceRebuild = false;
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	// Remove destroyed edges
	auto pred = [](const CMetalData::EdgeDesc& desc) {
		return true;
	};
	for (int index : unlinkClusters) {
		boost::remove_out_edge_if(index, pred, ownedClusters);
	}
	unlinkClusters.clear();

	// Add new edges to Kruskal graph
	for (int index : linkClusters) {
		CMetalData::Graph::out_edge_iterator outEdgeIt, outEdgeEnd;
		std::tie(outEdgeIt, outEdgeEnd) = boost::out_edges(index, clusterGraph);  // or boost::tie
		for (; outEdgeIt != outEdgeEnd; ++outEdgeIt) {
			const CMetalData::EdgeDesc& edgeId = *outEdgeIt;
			int idx0 = boost::target(edgeId, clusterGraph);
			if (linkedClusters[idx0]) {
				boost::add_edge(idx0, index, clusterGraph[edgeId], ownedClusters);

				CEnergyLink& link = boost::get(linkIt, edgeId);
				link.SetStartVertex(idx0);
			}
		}
	}
	linkClusters.clear();

	float width = circuit->GetTerrainManager()->GetTerrainWidth();
	float height = circuit->GetTerrainManager()->GetTerrainHeight();
	float baseWeight = width * width + height * height;
	float invBaseWeight = 1.0f / baseWeight;  // FIXME: only valid for 1 of the ally team
	const AIFloat3& basePos = circuit->GetSetupManager()->GetBasePos();
	for (const CMetalData::EdgeDesc& edgeId : spanningTree) {
		CEnergyLink& link = boost::get(linkIt, edgeId);
		if (link.IsFinished() || link.IsBeingBuilt()) {
			// Mark used edges as const
			ownedClusters[edgeId].weight = clusterGraph[edgeId].weight * invBaseWeight;
		} else if (!link.IsValid()) {
			ownedClusters[edgeId].weight = clusterGraph[edgeId].weight * baseWeight;
		} else {
			// Adjust weight by distance to base
			const CMetalData::SEdge& edge = clusterGraph[edgeId];
			ownedClusters[edgeId].weight = edge.weight * basePos.SqDistance2D(edge.center) * invBaseWeight;
		}
	}

	// Build Kruskal's minimum spanning tree
	spanningTree.clear();
	boost::property_map<CMetalData::Graph, float CMetalData::SEdge::*>::type w_map = boost::get(&CMetalData::SEdge::weight, ownedClusters);
	boost::kruskal_minimum_spanning_tree(ownedClusters, std::inserter(spanningTree, spanningTree.end()), boost::weight_map(w_map));
}


#ifdef DEBUG_VIS
void CEnergyGrid::UpdateVis()
{
	if (!isVis) {
		return;
	}

	Map* map = circuit->GetMap();
	Figure* fig = circuit->GetDrawer()->GetFigure();

	fig->Remove(figureFinishedId);
	fig->Remove(figureBuildId);
	fig->Remove(figureInvalidId);
	fig->Remove(figureGridId);
	// create new figure groups
	figureFinishedId = fig->DrawLine(ZeroVector, ZeroVector, 0.0f, false, FRAMES_PER_SEC * 300, 0);
	figureBuildId    = fig->DrawLine(ZeroVector, ZeroVector, 0.0f, false, FRAMES_PER_SEC * 300, 0);
	figureInvalidId  = fig->DrawLine(ZeroVector, ZeroVector, 0.0f, false, FRAMES_PER_SEC * 300, 0);
	figureGridId     = fig->DrawLine(ZeroVector, ZeroVector, 0.0f, false, FRAMES_PER_SEC * 300, 0);
	for (const CEnergyLink& link : links) {
		int figureId;
		float height = 20.0f;
		if (link.IsFinished()) {
			figureId = figureFinishedId;
			height = 18.0f;
		} else if (link.IsBeingBuilt()) {
			figureId = figureBuildId;
		} else if (!link.IsValid()) {
			figureId = figureInvalidId;
		} else {
			figureId = figureGridId;
			height = 18.0f;
		}
		AIFloat3 pos0 = link.GetV0()->pos;
		const AIFloat3 dir = (link.GetV1()->pos - pos0) / 10.0f;
		pos0.y = map->GetElevationAt(pos0.x, pos0.z) + height;
		for (int i = 0; i < 10; ++i) {
			AIFloat3 pos1 = pos0 + dir;
			pos1.y = map->GetElevationAt(pos1.x, pos1.z) + height;
			fig->DrawLine(pos0, pos1, 16.0f, false, FRAMES_PER_SEC * 300, figureId);
			pos0 = pos1;
		}
	}
	fig->SetColor(figureFinishedId, AIColor(0.1f, 0.3f, 1.0f), 255);
	fig->SetColor(figureBuildId,    AIColor(1.0f, 1.0f, 0.0f), 255);
	fig->SetColor(figureInvalidId,  AIColor(1.0f, 0.3f, 0.3f), 255);
	fig->SetColor(figureGridId,     AIColor(0.5f, 0.5f, 0.5f), 255);

	// Draw planned Kruskal
	fig->Remove(figureKruskalId);
	figureKruskalId = fig->DrawLine(ZeroVector, ZeroVector, 0.0f, false, FRAMES_PER_SEC * 300, 0);
	const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
	const CMetalData::Graph& clusterGraph = circuit->GetMetalManager()->GetGraph();
	for (const CMetalData::EdgeDesc& edge : spanningTree) {
		const AIFloat3& posFrom = clusters[boost::source(edge, clusterGraph)].geoCentr;
		const AIFloat3& posTo = clusters[boost::target(edge, clusterGraph)].geoCentr;
		AIFloat3 pos0 = posFrom;
		const AIFloat3 dir = (posTo - pos0) / 10.0f;
		pos0.y = map->GetElevationAt(pos0.x, pos0.z) + 19.0f;
		for (int i = 0; i < 10; ++i) {
			AIFloat3 pos1 = pos0 + dir;
			pos1.y = map->GetElevationAt(pos1.x, pos1.z) + 19.0f;
			fig->DrawLine(pos0, pos1, 16.0f, false, FRAMES_PER_SEC * 300, figureKruskalId);
			pos0 = pos1;
		}
	}
	fig->SetColor(figureKruskalId, AIColor(0.0f, 1.0f, 1.0f), 255);

	delete fig;
}

void CEnergyGrid::ToggleVis()
{
	if (toggleFrame >= circuit->GetLastFrame()) {
		return;
	}
	toggleFrame = circuit->GetLastFrame();

	isVis = !isVis;
	if (isVis) {
		UpdateVis();
	} else {
		Figure* fig = circuit->GetDrawer()->GetFigure();
		fig->Remove(figureFinishedId);
		fig->Remove(figureBuildId);
		fig->Remove(figureInvalidId);
		fig->Remove(figureGridId);
		fig->Remove(figureKruskalId);
		delete fig;
	}
}
#endif

} // namespace circuit
