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
#include "scheduler/Scheduler.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"
#include "json/json.h"
#include "lemon/kruskal.h"

#include "spring/SpringMap.h"

#include "AISCommands.h"
#include "Log.h"
#ifdef DEBUG_VIS
#include "Figure.h"
#endif

namespace circuit {

using namespace springai;

class CEnergyGrid::SpanningNode : public lemon::MapBase<CEnergyGrid::SpanningGraph::Node, bool> {
public:
	SpanningNode(const std::vector<bool>& lc, const std::vector<CEnergyNode*>& nodes)
		: linkedClusters(lc), nodes(nodes)
	{}
	Value operator[](Key k) const {
		return (*this)[CMetalData::ClusterGraph::id(k)];
	}
	Value operator[](int u) const {
		return linkedClusters[u] && !nodes[u]->IsBeingBuilt();
	}
private:
	const std::vector<bool>& linkedClusters;
	const std::vector<CEnergyNode*>& nodes;
};

class CEnergyGrid::SpanningLink : public lemon::MapBase<CEnergyGrid::SpanningGraph::Edge, bool> {
public:
	SpanningLink(const CEnergyGrid::SpanningTree& st, const std::vector<CEnergyLink>& links)
		: spanningTree(st), links(links)
	{}
	Value operator[](Key k) const {
		return spanningTree.find(k) != spanningTree.end();
	}
private:
	const CEnergyGrid::SpanningTree& spanningTree;
	const std::vector<CEnergyLink>& links;
};

class CEnergyGrid::DetectNode : public lemon::MapBase<CEnergyGrid::SpanningGraph::Node, bool> {
public:
	DetectNode(const CEnergyGrid::SpanningGraph& graph,
			const std::vector<CEnergyNode*>& nodes,
			const std::vector<CEnergyLink>& links, CCircuitAI* c)
		: graph(graph), nodes(nodes), links(links)
	{}
	Value operator[](Key k) const {
		const CEnergyNode* node = nodes[CMetalData::ClusterGraph::id(k)];
		if (node->IsBeingBuilt() || !node->IsValid()) {
			return false;
		}
		if (!node->IsMexed()) {
			return true;
		}
		if (node->IsFinished()) {
			return false;
		}
		CEnergyGrid::SpanningGraph::IncEdgeIt edgeIt(graph, k);
		for (; edgeIt != lemon::INVALID; ++ edgeIt) {
			const CEnergyLink& link = links[CMetalData::ClusterGraph::id(edgeIt)];
			if (!link.IsFinished() && link.IsValid()) {
				return false;
			}
		}
		return true;
	}
private:
	const CEnergyGrid::SpanningGraph& graph;
	const std::vector<CEnergyNode*>& nodes;
	const std::vector<CEnergyLink>& links;
};

class CEnergyGrid::DetectLink : public lemon::MapBase<CEnergyGrid::SpanningGraph::Edge, bool> {
public:
	DetectLink(const std::vector<CEnergyLink>& links) : links(links) {}
	Value operator[](Key k) const {
		const CEnergyLink& link = links[CMetalData::ClusterGraph::id(k)];
		return !link.IsFinished() && !link.IsBeingBuilt() && link.IsValid();
	}
private:
	const std::vector<CEnergyLink>& links;
};

CEnergyGrid::CEnergyGrid(CCircuitAI* circuit)
		: circuit(circuit)
		, markFrame(-1)
		, isForceRebuild(false)
		, ownedFilter(nullptr)
		, ownedClusters(nullptr)
		, edgeCosts(nullptr)
		, nodeFilter(nullptr)
		, linkFilter(nullptr)
		, spanningGraph(nullptr)
		, spanningBfs(nullptr)
#ifdef DEBUG_VIS
		, figureGridId(-1)
		, figureInvalidId(-1)
		, figureFinishedId(-1)
		, figureBuildId(-1)
		, figureKruskalId(-1)
		, isVis(false)
		, toggleFrame(-1)
#endif
{
	circuit->GetScheduler()->RunOnInit(CScheduler::GameJob(&CEnergyGrid::Init, this));

	for (CCircuitDef& cdef : circuit->GetCircuitDefs()) {
		const std::map<std::string, std::string>& customParams = cdef.GetDef()->GetCustomParams();
		auto it = customParams.find("pylonrange");
		if (it != customParams.end()) {
			pylonRanges[cdef.GetId()] = utils::string_to_float(it->second) - 1.f;
			cdef.SetIsPylon(true);
		}
	}

	ReadConfig();
}

CEnergyGrid::~CEnergyGrid()
{
	delete ownedFilter;
	delete ownedClusters;
	delete edgeCosts;

	delete nodeFilter;
	delete linkFilter;
	delete spanningGraph;
	delete spanningBfs;

	utils::free_clear(nodes);
}

void CEnergyGrid::ReadConfig()
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();
	const Json::Value& pylon = root["economy"]["energy"]["pylon"];
	for (const Json::Value& pyl : pylon) {
		CCircuitDef* cdef = circuit->GetCircuitDef(pyl.asCString());
		if (cdef == nullptr) {
			circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), pyl.asCString());
		} else {
			rangePylons[pylonRanges[cdef->GetId()]] = cdef->GetId();
		}
	}
}

void CEnergyGrid::Init()
{
	CMetalManager* metalMgr = circuit->GetMetalManager();
	const CMetalData::Metals& spots = metalMgr->GetSpots();
	const CMetalData::Clusters& clusters = metalMgr->GetClusters();
	const CMetalData::ClusterGraph& clusterGraph = metalMgr->GetClusterGraph();

	ownedFilter = new OwnedFilter(clusterGraph, false);
	ownedClusters = new OwnedGraph(clusterGraph, *ownedFilter);
	edgeCosts = new CMetalData::ClusterCostMap(clusterGraph);

	nodeFilter = new SpanningNode(linkedClusters, nodes);
	linkFilter = new SpanningLink(spanningTree, links);
	spanningGraph = new SpanningGraph(clusterGraph, *nodeFilter, *linkFilter);
	spanningBfs = new SpanningBFS(*spanningGraph);

	linkedClusters.resize(clusters.size(), false);
	const CMetalData::ClusterCostMap& costs = metalMgr->GetClusterEdgeCosts();

	auto closestSpot = [&spots](const CMetalData::MetalIndices& indices, const AIFloat3& pos) {
		int idxSpot = -1;
		float minDist = std::numeric_limits<float>::max();
		for (int i : indices) {
			float dist = spots[i].position.SqDistance2D(pos);
			if (dist < minDist) {
				minDist = dist;
				idxSpot = i;
			}
		}
		return idxSpot;
	};

	links.reserve(clusterGraph.edgeNum());
	for (int i = 0; i < clusterGraph.edgeNum(); ++i) {
		CMetalData::ClusterGraph::Edge edge = clusterGraph.edgeFromId(i);
		// NOTE: CMetalManager::GetClusterEdgeCosts() is not the same as distance between cluster mexes
		(*edgeCosts)[edge] = costs[edge];
		int idx0 = clusterGraph.id(clusterGraph.u(edge));
		int idx1 = clusterGraph.id(clusterGraph.v(edge));
		int idxSpot0 = closestSpot(clusters[idx0].idxSpots, clusters[idx1].position);
		int idxSpot1 = closestSpot(clusters[idx1].idxSpots, spots[idxSpot0].position);
		links.emplace_back(idx0, spots[idxSpot0].position, idx1, spots[idxSpot1].position);
	}

	nodes.reserve(clusterGraph.nodeNum());
	for (int i = 0; i < clusterGraph.nodeNum(); ++i) {
		nodes.push_back(new CEnergyNode(i, clusters[i], spots));
	}
}

void CEnergyGrid::Update()
{
	if (markFrame /*+ FRAMES_PER_SEC*/ >= circuit->GetLastFrame()) {
		return;
	}
	markFrame = circuit->GetLastFrame();

	circuit->UpdateFriendlyUnits();
	const CAllyTeam::AllyUnits& friendlies = circuit->GetFriendlyUnits();
	static std::vector<CAllyUnit*> tmpMexes;  // NOTE: micro-opt
	static std::vector<CAllyUnit*> tmpPylons;  // NOTE: micro-opt
//	tmpMexes.reserve(friendlies.size());
//	tmpPylons.reserve(friendlies.size());
	for (auto& kv : friendlies) {
		CAllyUnit* unit = kv.second;
		if (unit->GetUnit()->IsBeingBuilt()) {
			continue;
		}
		if (unit->GetCircuitDef()->IsMex()) {
			tmpMexes.push_back(unit);
		}
		if (unit->GetCircuitDef()->IsPylon()) {
			tmpPylons.push_back(unit);
		}
	}

	circuit->GetMetalManager()->MarkAllyMexes(tmpMexes);
	MarkClusters();
	MarkAllyPylons(tmpPylons);
	CheckGrid();
	RebuildTree();

	tmpMexes.clear();
	tmpPylons.clear();
}

IGridLink* CEnergyGrid::GetLinkToBuild(CCircuitDef*& outDef, AIFloat3& outPos)
{
	/*
	 * Detect link to build
	 */
	const AIFloat3& pos = circuit->GetSetupManager()->GetBasePos();
	int index = circuit->GetMetalManager()->FindNearestCluster(pos);
	if (index < 0 || !(*nodeFilter)[index]) {
		return nullptr;
	}

	DetectNode goalNode(*spanningGraph, nodes, links, circuit);
	DetectLink goalLink(links);
	spanningBfs->init();
	spanningBfs->addSource(spanningGraph->nodeFromId(index));
	// NOTE: usually 'start' doesn't examine source node, except this specific version
	auto target = spanningBfs->start(goalNode, goalLink);
	if (target.first != lemon::INVALID) {
		CEnergyNode* node = nodes[spanningGraph->id(target.first)];
		return FindNodeDef(outDef, outPos, node);
	}
	if (target.second != lemon::INVALID) {
		CEnergyLink* link = &links[spanningGraph->id(target.second)];
		return FindLinkDef(outDef, outPos, link);
	}
	return nullptr;
}

float CEnergyGrid::GetPylonRange(CCircuitDef::Id defId)
{
	auto it = pylonRanges.find(defId);
	return (it != pylonRanges.end()) ? it->second : .0f;
}

CEnergyNode* CEnergyGrid::FindNodeDef(CCircuitDef*& outDef, AIFloat3& outPos, CEnergyNode* node)
{
	outDef = nullptr;
	outPos = -RgtVector;
	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	const float searchRadius = economyMgr->GetPylonRange() * 0.5f;
	const int frame = circuit->GetLastFrame();
	const float metalIncome = economyMgr->GetAvgMetalIncome();

	const CCircuitDef* pylonDef = economyMgr->GetPylonDef();
	if (node->IsPylonable(economyMgr->GetPylonRange())
		&& pylonDef->IsAvailable(frame)
		&& (metalIncome > pylonDef->GetCostM() * 0.15f))
	{
		outDef = const_cast<CCircuitDef*>(pylonDef);
		outPos = circuit->GetTerrainManager()->FindBuildSite(outDef, node->GetCenterPos(), searchRadius, UNIT_COMMAND_BUILD_NO_FACING);
		return node;
	}

	const CEnergyNode::SPylon& pylon0 = node->GetSourceHead();
	const CEnergyNode::SPylon& pylon1 = node->GetTargetHead();

	decltype(rangePylons) candDefs = rangePylons;
	while (!candDefs.empty()) {
		CCircuitDef::Id defId = -1;
		float range;
		auto it = candDefs.rbegin();
		for (; it != candDefs.rend(); ++it) {
			defId = it->second;
			range = it->first;
			const float dist = pylon0.range + pylon1.range + range * 1.2f;
			if (pylon0.pos.SqDistance2D(pylon1.pos) > SQUARE(dist)) {
				break;
			}
		}

		outDef = circuit->GetCircuitDef(defId);
		if ((outDef == nullptr) || !outDef->IsAvailable(frame)
			|| ((metalIncome < outDef->GetCostM() * 0.2f) && (outDef->GetCostM() > 100.f)))
		{
			candDefs.erase(range);
			continue;
		}

		AIFloat3 dir = pylon1.pos - pylon0.pos;
		AIFloat3 sweetPos;
		const float dist = pylon0.range + pylon1.range + range * 1.95f;
		if (dir.SqLength2D() < SQUARE(dist)) {
			sweetPos = (pylon0.pos + dir.Normalize2D() * (pylon0.range - pylon1.range) + pylon1.pos) * 0.5f;
		} else {
			sweetPos = pylon0.pos + dir.Normalize2D() * (pylon0.range + range) * 0.95f;
		}

		outPos = circuit->GetTerrainManager()->FindBuildSite(outDef, sweetPos, searchRadius, UNIT_COMMAND_BUILD_NO_FACING);
		if (utils::is_valid(outPos) && circuit->GetBuilderManager()->IsBuilderInArea(outDef, outPos)) {
			break;
		} else {
			outPos = -RgtVector;
//			candDefs.erase(std::next(it).base());  // http://stackoverflow.com/questions/1830158/how-to-call-erase-with-a-reverse-iterator
			candDefs.erase(range);
		}
	}

	return node;
}

CEnergyLink* CEnergyGrid::FindLinkDef(CCircuitDef*& outDef, AIFloat3& outPos, CEnergyLink* link)
{
	outDef = nullptr;
	outPos = -RgtVector;
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	const float searchRadius = economyMgr->GetPylonRange() * 0.5f;

	const CEnergyLink::SPylon* pylon0 = link->GetSourceHead();
	const CEnergyLink::SPylon* pylon1 = link->GetTargetHead();

	const float metalIncome = economyMgr->GetAvgMetalIncome();
	const int frame = circuit->GetLastFrame();
	decltype(rangePylons) candDefs = rangePylons;
	while (!candDefs.empty()) {
		CCircuitDef::Id defId = -1;
		float range;
		auto it = candDefs.rbegin();
		for (; it != candDefs.rend(); ++it) {
			defId = it->second;
			range = it->first;
			const float dist = pylon0->range + pylon1->range + range * 1.2f;
			if (pylon0->pos.SqDistance2D(pylon1->pos) > SQUARE(dist)) {
				break;
			}
		}

		outDef = circuit->GetCircuitDef(defId);
		if ((outDef == nullptr) || !outDef->IsAvailable(frame)
			|| ((metalIncome < outDef->GetCostM() * 0.2f) && (outDef->GetCostM() > 100.f)))
		{
			candDefs.erase(range);
			continue;
		}

		AIFloat3 dir = pylon1->pos - pylon0->pos;
		AIFloat3 sweetPos;
		const float dist = pylon0->range + pylon1->range + range * 1.95f;
		if (dir.SqLength2D() < SQUARE(dist)) {
			sweetPos = (pylon0->pos + dir.Normalize2D() * (pylon0->range - pylon1->range) + pylon1->pos) * 0.5f;
		} else {
			sweetPos = pylon0->pos + dir.Normalize2D() * (pylon0->range + range) * 0.95f;
		}
		// TODO: replace line placement by pathfinder
		if (!circuit->GetMap()->IsPossibleToBuildAt(outDef->GetDef(), sweetPos, 0)) {
			candDefs.erase(range);
			continue;
		}

		outPos = terrainMgr->FindBuildSite(outDef, sweetPos, searchRadius, UNIT_COMMAND_BUILD_NO_FACING);
		if (utils::is_valid(outPos) && builderMgr->IsBuilderInArea(outDef, outPos)) {
			break;
		} else {
			outPos = -RgtVector;
//			candDefs.erase(std::next(it).base());  // http://stackoverflow.com/questions/1830158/how-to-call-erase-with-a-reverse-iterator
			candDefs.erase(range);
		}
	}

	return link;
}

void CEnergyGrid::MarkAllyPylons(const std::vector<CAllyUnit*>& pylons)
{
	decltype(markedPylons) prevUnits = std::move(markedPylons);
	markedPylons.clear();
	auto first1  = pylons.begin();
	auto last1   = pylons.end();
	auto first2  = prevUnits.begin();
	auto last2   = prevUnits.end();
	auto d_first = std::back_inserter(markedPylons);
	auto addPylon = [&d_first, this](CAllyUnit* unit) {
		std::vector<CEnergyNode*> ns;
		AddPylon(unit->GetId(), unit->GetCircuitDef()->GetId(), unit->GetPos(circuit->GetLastFrame()), ns);
		*d_first++ = std::make_pair(unit->GetId(), ns);
	};
	auto delPylon = [this](const std::pair<ICoreUnit::Id, std::vector<CEnergyNode*>>& pylonId) {
		RemovePylon(pylonId);
	};

	// @see std::set_symmetric_difference + std::set_intersection
	while (first1 != last1) {
		if (first2 == last2) {
			addPylon(*first1);  // everything else in first1..last1 is new units
			while (++first1 != last1) {
				addPylon(*first1);
			}
			break;
		}

		if ((*first1)->GetId() < first2->first) {
			addPylon(*first1);  // new unit
			++first1;  // advance pylons
		} else {
			if (first2->first < (*first1)->GetId()) {
				delPylon(*first2);  // dead unit
			} else {
				*d_first++ = *first2;  // old unit
				++first1;  // advance pylons
			}
			++first2;  // advance prevUnits
		}
	}
	while (first2 != last2) {  // everything else in first2..last2 is dead units
		delPylon(*first2++);
	}
}

void CEnergyGrid::AddPylon(const ICoreUnit::Id unitId, const CCircuitDef::Id defId, const AIFloat3& pos,
		std::vector<CEnergyNode*>& outNodes)
{
	CMetalManager* metalMgr = circuit->GetMetalManager();
	const CMetalData::ClusterGraph& clusterGraph = metalMgr->GetClusterGraph();

	// Find edges to which building belongs to
	const float range = pylonRanges[defId];
	CMetalData::ClusterGraph::EdgeIt edgeIt(clusterGraph);
	for (; edgeIt != lemon::INVALID; ++edgeIt) {
		int edgeIdx = clusterGraph.id(edgeIt);
		CEnergyLink& link = links[edgeIdx];
		const AIFloat3& P0 = link.GetSourcePos();
		const AIFloat3& P1 = link.GetTargetPos();

		const AIFloat3 midPos = (P0 + P1) * 0.5f;
		if (midPos.SqDistance2D(pos) < SQUARE(midPos.distance2D(P1) + range)) {
			link.AddPylon(unitId, pos, range);
			linkPylons.insert(edgeIdx);
		}
	}

	CMetalData::IndicesDists indices;
	metalMgr->FindSpotsInRadius(pos, range, indices);
	if (indices.empty()) {
		int index = metalMgr->FindNearestCluster(pos);
		if (index < 0) {
			return;
		}
		nodes[index]->AddPylon(unitId, pos, range);
		outNodes.push_back(nodes[index]);
		linkNodes.insert(nodes[index]);
	} else {
		for (const std::pair<int, float>& p : indices) {
			int index = metalMgr->GetCluster(p.first);
			if (nodes[index]->AddPylon(unitId, pos, range)) {
				outNodes.push_back(nodes[index]);
				linkNodes.insert(nodes[index]);
			}
		}
	}
}

void CEnergyGrid::RemovePylon(const std::pair<ICoreUnit::Id, std::vector<CEnergyNode*>>& pylonId)
{
	for (int i = 0; i < (int)links.size(); ++i) {
		if (links[i].RemovePylon(pylonId.first)) {
			unlinkPylons.insert(i);
		}
	}

	for (CEnergyNode* node : pylonId.second) {
		if (node->RemovePylon(pylonId.first)) {
			linkNodes.insert(node);
		}
	}
}

void CEnergyGrid::CheckGrid()
{
	if (linkPylons.empty() && unlinkPylons.empty()
		&& linkNodes.empty())
	{
		return;
	}

	for (const int edgeIdx : linkPylons) {
		CEnergyLink& link = links[edgeIdx];
		if (link.IsFinished()) {
			continue;
		}
		link.CheckConnection();
	}
	linkPylons.clear();

	for (const int edgeIdx : unlinkPylons) {
		links[edgeIdx].CheckConnection();  // calculates source, target
	}
	unlinkPylons.clear();

	for (CEnergyNode* node : linkNodes) {
		node->CheckConnection();
	}
	linkNodes.clear();
}

void CEnergyGrid::MarkClusters()
{
	const int size = circuit->GetMetalManager()->GetClusters().size();
	for (int index = 0; index < size; ++index) {
		bool isOur = circuit->GetMetalManager()->IsClusterFinished(index);
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
	CMetalManager* metalMgr = circuit->GetMetalManager();
	const CMetalData::ClusterGraph& clusterGraph = metalMgr->GetClusterGraph();

	// Remove destroyed edges
	for (int index : unlinkClusters) {
		OwnedGraph::Node node = ownedClusters->nodeFromId(index);
		ownedClusters->disable(node);
	}
	unlinkClusters.clear();

	// Add new edges to Kruskal graph
	for (int index : linkClusters) {
		CMetalData::ClusterGraph::Node node = clusterGraph.nodeFromId(index);
		ownedClusters->enable(node);
		CMetalData::ClusterGraph::IncEdgeIt edgeIt(clusterGraph, node);
		for (; edgeIt != lemon::INVALID; ++edgeIt) {
			int idx0 = clusterGraph.id(clusterGraph.oppositeNode(node, edgeIt));
			if (linkedClusters[idx0]) {
				CEnergyLink& link = links[clusterGraph.id(edgeIt)];
				link.SetSource(idx0);
			}
		}
	}
	linkClusters.clear();

	const CMetalData::ClusterCostMap& costs = metalMgr->GetClusterEdgeCosts();
	for (const CMetalData::ClusterGraph::Edge edge : spanningTree) {
		CEnergyLink& link = links[clusterGraph.id(edge)];
		if (link.IsFinished() || link.IsBeingBuilt()) {
			// Mark used edges as const
			(*edgeCosts)[edge] = costs[edge] * MIN_COSTMOD;
		} else if (!link.IsValid()) {
			(*edgeCosts)[edge] = costs[edge] / MIN_COSTMOD;
		} else {
			(*edgeCosts)[edge] = costs[edge] * link.GetCostMod();
		}
	}

	// Build Kruskal's minimum spanning tree
	spanningTree.clear();
	lemon::kruskal(*ownedClusters, *edgeCosts, std::inserter(spanningTree, spanningTree.end()));
}

#ifdef DEBUG_VIS
void CEnergyGrid::UpdateVis()
{
	if (!isVis) {
		return;
	}

	CMap* map = circuit->GetMap();
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
		AIFloat3 pos0 = link.GetSourcePos();
		const AIFloat3 dir = (link.GetTargetPos() - pos0) / 10;
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
	const CMetalData::ClusterGraph& clusterGraph = circuit->GetMetalManager()->GetClusterGraph();
	for (const CMetalData::ClusterGraph::Edge edge : spanningTree) {
		const AIFloat3& posFrom = clusters[clusterGraph.id(clusterGraph.u(edge))].position;
		const AIFloat3& posTo = clusters[clusterGraph.id(clusterGraph.v(edge))].position;
		AIFloat3 pos0 = posFrom;
		const AIFloat3 dir = (posTo - pos0) / 10;
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

void CEnergyGrid::DrawNodePylons(const AIFloat3& pos)
{
	int index = circuit->GetMetalManager()->FindNearestCluster(pos);
	for (const auto& kv : nodes[index]->GetPylons()) {
		CAllyUnit* unit = circuit->GetFriendlyUnit(kv.first);
		const AIFloat3& p = unit->GetPos(circuit->GetLastFrame());
		circuit->GetDrawer()->DeletePointsAndLines(p);
	}
	for (const auto& kv : nodes[index]->GetPylons()) {
		CAllyUnit* unit = circuit->GetFriendlyUnit(kv.first);
		const AIFloat3& p = unit->GetPos(circuit->GetLastFrame());
		circuit->GetDrawer()->AddPoint(p, utils::int_to_string(index, "n: %i").c_str());
	}
}

void CEnergyGrid::DrawLinkPylons(const AIFloat3& pos)
{
	float minDist = std::numeric_limits<float>::max();
	int index = -1;
	for (int i = 0; i < (int)links.size(); ++i) {
		float dist = ((links[i].GetSourcePos() + links[i].GetTargetPos()) * 0.5f).SqDistance2D(pos);
		if (dist < minDist) {
			minDist = dist;
			index = i;
		}
	}
	for (const auto& kv : links[index].GetPylons()) {
		const AIFloat3& p = kv.second->pos;
		circuit->GetDrawer()->DeletePointsAndLines(p);
	}
	for (const auto& kv : links[index].GetPylons()) {
		const AIFloat3& p = kv.second->pos;
		circuit->GetDrawer()->AddPoint(p, utils::int_to_string(index, "l: %i").c_str());
	}
}
#endif

} // namespace circuit
