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
#include "json/json.h"
#include "lemon/kruskal.h"

#include "AISCommands.h"
#include "Log.h"
#ifdef DEBUG_VIS
#include "Figure.h"
#endif

namespace circuit {

using namespace springai;

class CEnergyGrid::SpanningLink : public lemon::MapBase<CEnergyGrid::OwnedGraph::Edge, bool> {
public:
	SpanningLink(const CEnergyGrid::SpanningTree& st, const std::vector<CEnergyLink>& links)
		: spanningTree(st)
		, links(links)
	{}
	Value operator[](Key k) const {
		// NOTE: check for link.IsBeingBuilt solves vertex's pylon duplicates, but slows down grid construction
		return (spanningTree.find(k) != spanningTree.end())
				&& !links[CMetalData::Graph::id(k)].IsBeingBuilt();
	}
private:
	const CEnergyGrid::SpanningTree& spanningTree;
	const std::vector<CEnergyLink>& links;
};

class CEnergyGrid::DetectLink : public lemon::MapBase<CEnergyGrid::OwnedGraph::Edge, bool> {
public:
	DetectLink(const std::vector<CEnergyLink>& links) : links(links) {}
	bool operator[](Key k) const {
		const CEnergyLink& link = links[CMetalData::Graph::id(k)];
		return !link.IsFinished() && link.IsValid();
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
		, spanningFilter(nullptr)
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
	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>(&CEnergyGrid::Init, this));

	const CCircuitAI::CircuitDefs& allDefs = circuit->GetCircuitDefs();
	for (auto& kv : allDefs) {
		const std::map<std::string, std::string>& customParams = kv.second->GetUnitDef()->GetCustomParams();
		auto it = customParams.find("pylonrange");
		if (it != customParams.end()) {
			pylonRanges[kv.first] = utils::string_to_float(it->second) - 1.f;
		}
	}

	ReadConfig();
}

CEnergyGrid::~CEnergyGrid()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);

	delete ownedFilter;
	delete ownedClusters;
	delete edgeCosts;

	delete spanningFilter;
	delete spanningGraph;
	delete spanningBfs;
}

void CEnergyGrid::Update()
{
	if (markFrame /*+ FRAMES_PER_SEC*/ >= circuit->GetLastFrame()) {
		return;
	}
	markFrame = circuit->GetLastFrame();

	circuit->UpdateFriendlyUnits();
	CCircuitDef* mexDef = circuit->GetEconomyManager()->GetMexDef();
	const CAllyTeam::Units& friendlies = circuit->GetFriendlyUnits();
	static std::vector<CAllyUnit*> tmpMexes;  // NOTE: micro-opt
	static std::vector<CAllyUnit*> tmpPylons;  // NOTE: micro-opt
//	tmpMexes.reserve(friendlies.size());
//	tmpPylons.reserve(friendlies.size());
	for (auto& kv : friendlies) {
		CAllyUnit* unit = kv.second;
		if (*unit->GetCircuitDef() == *mexDef) {
			tmpMexes.push_back(unit);
		} else if (pylonRanges.find(unit->GetCircuitDef()->GetId()) != pylonRanges.end()) {
			tmpPylons.push_back(unit);
		}
	}

	circuit->GetMetalManager()->MarkAllyMexes(tmpMexes);
	MarkClusters();
	RebuildTree();

	MarkAllyPylons(tmpPylons);
	CheckGrid();

	tmpMexes.clear();
	tmpPylons.clear();
}

CEnergyLink* CEnergyGrid::GetLinkToBuild(CCircuitDef*& outDef, AIFloat3& outPos)
{
	/*
	 * Detect link to build
	 */
	const AIFloat3& pos = circuit->GetSetupManager()->GetBasePos();
	int index = circuit->GetMetalManager()->FindNearestCluster(pos);
	if (index < 0) {
		return nullptr;
	}

	DetectLink goal(links);
	spanningBfs->init();
	spanningBfs->addSource(spanningGraph->nodeFromId(index));
	CMetalData::Graph::Edge target = spanningBfs->startEdge(goal);
	if (target == lemon::INVALID) {
		return nullptr;
	}
	CEnergyLink* link = &links[spanningGraph->id(target)];

	/*
	 * Find best build def and position
	 */
	outDef = nullptr;
	const float searchRadius = circuit->GetEconomyManager()->GetPylonRange() * 0.5f;

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

	const int frame = circuit->GetLastFrame();
	decltype(rangePylons) candDefs = rangePylons;
	while (!candDefs.empty()) {
		CCircuitDef::Id defId = -1;
		float range;
		auto it = candDefs.rbegin();
		for (; it != candDefs.rend(); ++it) {
			defId = it->second;
			range = it->first;
			const float dist = pylon0->range + pylon1->range + range;
			if (pylon0->pos.SqDistance2D(pylon1->pos) > SQUARE(dist)) {
				break;
			}
		}

		outDef = circuit->GetCircuitDef(defId);
		if ((outDef == nullptr) || !outDef->IsAvailable(frame)) {
			outPos = -RgtVector;
			candDefs.erase(range);
			continue;
		}

		AIFloat3 dir = pylon1->pos - pylon0->pos;
		AIFloat3 sweetPos;
		const float dist = pylon0->range + pylon1->range + range * 2.0f;
		if (dir.SqLength2D() < SQUARE(dist)) {
			sweetPos = (pylon0->pos + dir.Normalize2D() * (pylon0->range - pylon1->range) + pylon1->pos) * 0.5f;
		} else {
			sweetPos = pylon0->pos + dir.Normalize2D() * (pylon0->range + range) * 0.95f;
		}

		outPos = circuit->GetTerrainManager()->FindBuildSite(outDef, sweetPos, searchRadius, UNIT_COMMAND_BUILD_NO_FACING);
		if (utils::is_valid(outPos)) {
			break;
		} else {
//			candDefs.erase(std::next(it).base());  // http://stackoverflow.com/questions/1830158/how-to-call-erase-with-a-reverse-iterator
			candDefs.erase(range);
		}
	}

	return link;
}

float CEnergyGrid::GetPylonRange(CCircuitDef::Id defId)
{
	auto it = pylonRanges.find(defId);
	return (it != pylonRanges.end()) ? it->second : .0f;
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
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	ownedFilter = new OwnedFilter(clusterGraph, false);
	ownedClusters = new OwnedGraph(clusterGraph, *ownedFilter);
	edgeCosts = new CMetalData::WeightMap(clusterGraph);

	spanningFilter = new SpanningLink(spanningTree, links);
	spanningGraph = new SpanningGraph(metalManager->GetGraph(), *spanningFilter);
	spanningBfs = new SpanningBFS(*spanningGraph);

	linkedClusters.resize(clusters.size(), false);

	links.reserve(clusterGraph.edgeNum());
	for (int i = 0; i < clusterGraph.edgeNum(); ++i) {
		CMetalData::Graph::Edge edge = clusterGraph.edgeFromId(i);
		int idx0 = clusterGraph.id(clusterGraph.u(edge));
		int idx1 = clusterGraph.id(clusterGraph.v(edge));
		links.emplace_back(idx0, clusters[idx0].position, idx1, clusters[idx1].position);
	}
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
		*d_first++ = unit->GetId();
		AddPylon(unit->GetId(), unit->GetCircuitDef()->GetId(), unit->GetPos(this->circuit->GetLastFrame()));
	};
	auto delPylon = [this](const ICoreUnit::Id unitId) {
		RemovePylon(unitId);
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

		if ((*first1)->GetId() < *first2) {
			addPylon(*first1);  // new unit
			++first1;  // advance pylons
		} else {
			if (*first2 < (*first1)->GetId()) {
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

void CEnergyGrid::AddPylon(ICoreUnit::Id unitId, CCircuitDef::Id defId, const AIFloat3& pos)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	// Find edges to which building belongs to
	float range = pylonRanges[defId];
	float sqRange = SQUARE(range);
	CMetalData::Graph::EdgeIt edgeIt(clusterGraph);
	for (; edgeIt != lemon::INVALID; ++edgeIt) {
		int idxSource = clusterGraph.id(clusterGraph.u(edgeIt));
		int idxTarget = clusterGraph.id(clusterGraph.v(edgeIt));
		const AIFloat3& P0 = clusters[idxSource].position;
		const AIFloat3& P1 = clusters[idxTarget].position;

		if ((P0.SqDistance2D(pos) < sqRange) || (P1.SqDistance2D(pos) < sqRange) ||
			(((P0 + P1) * 0.5f).SqDistance2D(pos) < P0.SqDistance2D(P1) * 0.25f))
		{
			int edgeIdx = clusterGraph.id(edgeIt);
			CEnergyLink& link = links[edgeIdx];
			link.AddPylon(unitId, pos, range);
			linkPylons.insert(edgeIdx);
		}
	}
}

void CEnergyGrid::RemovePylon(ICoreUnit::Id unitId)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	// Find edges to which building belongs to
	CMetalData::Graph::EdgeIt edgeIt(clusterGraph);
	for (; edgeIt != lemon::INVALID; ++edgeIt) {
		int edgeIdx = clusterGraph.id(edgeIt);
		CEnergyLink& link = links[edgeIdx];
		if (link.RemovePylon(unitId)) {
			unlinkPylons.insert(edgeIdx);
		}
	}
}

void CEnergyGrid::CheckGrid()
{
	if (linkPylons.empty() && unlinkPylons.empty()) {
		return;
	}

	for (const int edgeIdx : linkPylons) {
		CEnergyLink& link = links[edgeIdx];
		if (link.IsFinished() || !link.IsValid()) {
			continue;
		}
		link.CheckConnection();
	}
	linkPylons.clear();

	for (const int edgeIdx : unlinkPylons) {
		CEnergyLink& link = links[edgeIdx];
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
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	// Remove destroyed edges
	for (int index : unlinkClusters) {
		OwnedGraph::Node node = ownedClusters->nodeFromId(index);
		ownedClusters->disable(node);
		OwnedGraph::IncEdgeIt edgeIt(*ownedClusters, node);
		for (; edgeIt != lemon::INVALID; ++edgeIt) {
			spanningTree.erase(edgeIt);
		}
	}
	unlinkClusters.clear();

	// Add new edges to Kruskal graph
	for (int index : linkClusters) {
		CMetalData::Graph::Node node = clusterGraph.nodeFromId(index);
		ownedClusters->enable(node);
		CMetalData::Graph::IncEdgeIt edgeIt(clusterGraph, node);
		for (; edgeIt != lemon::INVALID; ++edgeIt) {
			int idx0 = clusterGraph.id(clusterGraph.oppositeNode(node, edgeIt));
			if (linkedClusters[idx0]) {
				CEnergyLink& link = links[clusterGraph.id(edgeIt)];
				link.SetStartVertex(idx0);
			}
		}
	}
	linkClusters.clear();

	const CMetalData::WeightMap& weights = metalManager->GetWeights();
	const CMetalData::CenterMap& centers = metalManager->GetCenters();
	float width = circuit->GetTerrainManager()->GetTerrainWidth();
	float height = circuit->GetTerrainManager()->GetTerrainHeight();
	float baseWeight = width * width + height * height;
	float invBaseWeight = 1.0f / baseWeight;  // FIXME: only valid for 1 of the ally team
	const AIFloat3& basePos = circuit->GetSetupManager()->GetBasePos();

	for (const CMetalData::Graph::Edge edge : spanningTree) {
		CEnergyLink& link = links[clusterGraph.id(edge)];
		if (link.IsFinished() || link.IsBeingBuilt()) {
			// Mark used edges as const
			(*edgeCosts)[edge] = weights[edge] * invBaseWeight;
		} else if (!link.IsValid()) {
			(*edgeCosts)[edge] = weights[edge] * baseWeight;
		} else {
			// Adjust weight by distance to base
			(*edgeCosts)[edge] = weights[edge] * basePos.SqDistance2D(centers[edge]) * invBaseWeight;
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
		const AIFloat3 dir = (link.GetV1()->pos - pos0) / 10;
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
	for (const CMetalData::Graph::Edge edge : spanningTree) {
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
#endif

} // namespace circuit
