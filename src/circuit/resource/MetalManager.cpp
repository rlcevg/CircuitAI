/*
 * MetalManager.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "resource/MetalManager.h"
#include "map/ThreatMap.h"
#include "module/EconomyManager.h"
#include "setup/SetupManager.h"  // Only for json
#include "scheduler/Scheduler.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/math/RagMatrix.h"
#include "util/math/Region.h"
#include "util/Utils.h"
#include "json/json.h"

#include "spring/SpringMap.h"

#include "Game.h"
#include "MoveData.h"
#include "Pathing.h"

namespace circuit {

using namespace springai;

class CMetalManager::SafeCluster : public lemon::MapBase<ClusterGraph::Node, bool> {
public:
	SafeCluster(CThreatMap* tm, const CMetalData::Clusters& cs)
		: threatMap(tm)
		, clusters(cs)
	{}
	Value operator[](Key k) const {
		return (*this)[CMetalData::ClusterGraph::id(k)];
	}
	Value operator[](int u) const {
		return threatMap->GetThreatAt(clusters[u].position) <= THREAT_MIN;
	}
private:
	CThreatMap* threatMap;
	const CMetalData::Clusters& clusters;
};

class CMetalManager::BuildCluster : public lemon::MapBase<ClusterGraph::Node, bool> {
public:
	BuildCluster(CMetalManager* mgr, CMetalData::PointPredicate& pred, std::vector<int>& outIdxs)
		: manager(mgr)
		, predicate(pred)
		, indices(outIdxs)
	{}
	Value operator[](Key k) const {
		const int u = CMetalData::ClusterGraph::id(k);
		if (manager->IsClusterQueued(u) || manager->IsClusterFinished(u)) {
			return false;
		}
		for (int index : manager->GetClusters()[u].idxSpots) {
			if (predicate(index)) {
				indices.push_back(index);
			}
		}
		return !indices.empty();
	}
private:
	CMetalManager* manager;
	CMetalData::PointPredicate predicate;
	std::vector<int>& indices;
};

class CMetalManager::UpgradeCluster : public lemon::MapBase<ClusterGraph::Node, bool> {
public:
	UpgradeCluster(CMetalManager* mgr, CMetalData::PointPredicate& pred, std::vector<int>& outIdxs)
		: manager(mgr)
		, predicate(pred)
		, indices(outIdxs)
	{}
	Value operator[](Key k) const {
		const int u = CMetalData::ClusterGraph::id(k);
		for (int index : manager->GetClusters()[u].idxSpots) {
			if (predicate(index)) {
				indices.push_back(index);
			}
		}
		return !indices.empty();
	}
private:
	CMetalManager* manager;
	CMetalData::PointPredicate predicate;
	std::vector<int>& indices;
};

std::vector<int> CMetalManager::indices;

CMetalManager::CMetalManager(CCircuitAI* circuit, CMetalData* metalData)
		: circuit(circuit)
		, metalData(metalData)
		, markFrame(-1)
		, threatFilter(nullptr)
		, filteredGraph(nullptr)
		, shortPath(nullptr)
{
	circuit->GetScheduler()->RunOnInit(CScheduler::GameJob(&CMetalManager::Init, this));

	if (!metalData->IsInitialized()) {
		// TODO: Add metal zone and no-metal-spots maps support
		ParseMetalSpots();
	}
	metalInfos.resize(metalData->GetSpots().size(), {true, -1});
}

CMetalManager::~CMetalManager()
{
	delete threatFilter;
	delete filteredGraph;
	delete shortPath;
}

void CMetalManager::Init()
{
	clusterInfos.resize(GetClusters().size(), {0});
	for (unsigned i = 0; i < clusterInfos.size(); ++i) {
		for (int idx : GetClusters()[i].idxSpots) {
			metalInfos[idx].clusterId = i;
		}
	}

	threatFilter = new SafeCluster(circuit->GetThreatMap(), GetClusters());
	filteredGraph = new ClusterGraph(GetClusterGraph(), *threatFilter);
	shortPath = new ShortPath(*filteredGraph, GetClusterEdgeCosts());
}

void CMetalManager::ParseMetalSpots()
{
	Game* game = circuit->GetGame();
	std::vector<CMetalData::SMetal> spots;

	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const bool calcMex = root["economy"].get("calc_mex", false).asBool();

	int mexCount = game->GetRulesParamFloat("mex_count", -1);
	if (calcMex || (mexCount <= 0)) {
		// FIXME: Replace metal-map workaround by own grid-spot generator
		CMap* map = circuit->GetMap();
		Resource* metalRes = circuit->GetEconomyManager()->GetMetalRes();
		F3Vec spotsPos;
		metalData->AnalyzeMap(circuit, map, metalRes, &circuit->GetGameAttribute()->GetTerrainData(), spotsPos);
		// NOTE: Ignores spots with income less than ~20% (50/255) of max spot
//		map->GetResourceMapSpotsPositions(metalRes, spotsPos);
		metalData->MakeResourcePoints(map, metalRes, spotsPos);
		const unsigned width = map->GetWidth();
		const unsigned height = map->GetHeight();
		const float mapSize = (width / 64) * (height / 64);
		const float offset = (float)rand() / RAND_MAX * 50.f - 25.f;
		//  8x8  ~  80 spots +-25
		// 24x24 ~ 240 spots +-25
		unsigned mCount = (240.f - 80.f) / (SQUARE(24.f) - SQUARE(8.f)) * (mapSize - SQUARE(8.f)) + 80.f + offset;
		unsigned inc;
		if (spotsPos.size() > mCount) {
			inc = spotsPos.size() / mCount;
			spots.reserve(mCount);
		} else {
			inc = 1;
			spots.reserve(spotsPos.size());
		}
		CCircuitDef* mexDef = circuit->GetEconomyManager()->GetSideInfo().mexDef;
//		const float extrRange = SQUARE_SIZE * 2;  // map->GetExtractorRadius(metalRes) / 2;  // mexDef->GetExtrRangeM();
//		CTerrainManager* terrainMgr = circuit->GetTerrainManager();
		const int xsize = mexDef->GetDef()->GetXSize();
		const int zsize = mexDef->GetDef()->GetZSize();
		for (unsigned i = 0; i < spotsPos.size(); i += inc) {
			AIFloat3& pos = spotsPos[i];
			// move position closer to center
//			pos.x += extrRange;  // xsize * SQUARE_SIZE / 2;
//			pos.z += extrRange;  // zsize * SQUARE_SIZE / 2;
//			CTerrainManager::SnapPosition(pos);
			const unsigned x1 = int(pos.x) / SQUARE_SIZE - (xsize / 2), x2 = x1 + xsize;
			const unsigned z1 = int(pos.z) / SQUARE_SIZE - (zsize / 2), z2 = z1 + zsize;
			if ((x1 < x2) && (x2 < width) && (z1 < z2) && (z2 < height)
				/*&& terrainMgr->CanBeBuiltAt(mexDef, pos)*/)  // FIXME: Water def?
			{
				const float y = map->GetElevationAt(pos.x, pos.z);
				spots.push_back({pos.y, AIFloat3(pos.x, y, pos.z)});
			}
		}

	} else {

		mexCount = std::min(mexCount, 1000);  // safety measure
		spots.resize(mexCount);
		const float extr = game->GetRulesParamFloat("base_extraction", 0.001f);
		for (int i = 0; i < mexCount; ++i) {
			std::string param;
			param = utils::int_to_string(i + 1, "mex_x%i");
			spots[i].position.x = game->GetRulesParamFloat(param.c_str(), 0.f);
			param = utils::int_to_string(i + 1, "mex_y%i");
			spots[i].position.y = game->GetRulesParamFloat(param.c_str(), 0.f);
			param = utils::int_to_string(i + 1, "mex_z%i");
			spots[i].position.z = game->GetRulesParamFloat(param.c_str(), 0.f);
			param = utils::int_to_string(i + 1, "mex_metal%i");
			spots[i].income = game->GetRulesParamFloat(param.c_str(), 0.f) / extr;
			CTerrainManager::SnapPosition(spots[i].position);
		}
	}

	metalData->Init(std::move(spots));
}

void CMetalManager::ClusterizeMetal(CCircuitDef* commDef)
{
	metalData->SetClusterizing(true);

	const float maxDistance = circuit->GetEconomyManager()->GetClusterRange();
	const CMetalData::Metals& spots = metalData->GetSpots();
	int nrows = spots.size();

	CRagMatrix<float> distmatrix(nrows);
	if (nrows <= 300) {
		MoveData* moveData = commDef->GetDef()->GetMoveData();
		int pathType = moveData->GetPathType();
		delete moveData;
		Pathing* pathing = circuit->GetPathing();
		for (int i = 1; i < nrows; i++) {
			for (int j = 0; j < i; j++) {
				float geomLength = spots[i].position.distance2D(spots[j].position);
				if (geomLength > 4 * maxDistance) {
					distmatrix(i, j) = geomLength;
				} else {
					float pathLength = pathing->GetApproximateLength(spots[i].position, spots[j].position, pathType, 0.0f);
					distmatrix(i, j) = (geomLength * 1.4f < pathLength) ? pathLength : geomLength;
				}
			}
		}
	} else {
		for (int i = 1; i < nrows; i++) {
			for (int j = 0; j < i; j++) {
				distmatrix(i, j) = spots[i].position.distance2D(spots[j].position);
			}
		}
	}

	metalData->Clusterize(maxDistance, distmatrix);
	metalData->Statistics();
}

void CMetalManager::FindWithinRangeSpots(const AIFloat3& posFrom, const AIFloat3& posTo,
										 CMetalData::IndicesDists& outIndices) const
{
	const AIFloat3 pos = (posFrom + posTo) / 2;
	const float radius = posFrom.distance2D(posTo) / 2;
	CMetalData::IndicesDists indices;
	FindSpotsInRadius(pos, radius, indices);
	const CMetalData::Metals& spots = GetSpots();
	const utils::SBox box(posFrom.x, posTo.x, posFrom.z, posTo.z);
	for (auto& kv : indices) {
		if (box.ContainsPoint(spots[kv.first].position)) {
			outIndices.push_back(kv);
		}
	}
}

void CMetalManager::SetOpenSpot(int index, bool value)
{
	if (metalInfos[index].isOpen != value) {
		metalInfos[index].isOpen = value;
		clusterInfos[metalInfos[index].clusterId].queuedCount += value ? -1 : 1;
	}
}

void CMetalManager::SetOpenSpot(const springai::AIFloat3& pos, bool value)
{
	int index = FindNearestSpot(pos);
	if (index != -1) {
		SetOpenSpot(index, value);
	}
}

bool CMetalManager::IsOpenSpot(const springai::AIFloat3& pos) const
{
	int index = FindNearestSpot(pos);
	if (index != -1) {
		return IsOpenSpot(index);
	}
	return false;
}

void CMetalManager::MarkAllyMexes()
{
	if (markFrame /*+ FRAMES_PER_SEC*/ >= circuit->GetLastFrame()) {
		return;
	}

	circuit->UpdateFriendlyUnits();
	const CAllyTeam::AllyUnits& friendlies = circuit->GetFriendlyUnits();
	static std::vector<CAllyUnit*> tmpMexes;  // NOTE: micro-opt
//	tmpMexes.reserve(friendlies.size());
	for (auto& kv : friendlies) {
		CAllyUnit* unit = kv.second;
		if (unit->GetCircuitDef()->IsMex()) {
			tmpMexes.push_back(unit);
		}
	}

	MarkAllyMexes(tmpMexes);

	tmpMexes.clear();
}

void CMetalManager::MarkAllyMexes(const std::vector<CAllyUnit*>& mexes)
{
	if (markFrame /*+ FRAMES_PER_SEC*/ >= circuit->GetLastFrame()) {
		return;
	}
	markFrame = circuit->GetLastFrame();

	decltype(markedMexes) prevUnits = std::move(markedMexes);
	markedMexes.clear();
	auto first1  = mexes.begin();
	auto last1   = mexes.end();
	auto first2  = prevUnits.begin();
	auto last2   = prevUnits.end();
	auto d_first = std::back_inserter(markedMexes);
	auto addMex = [&d_first, this](CAllyUnit* unit) {
		SMex mex;
		mex.index = FindNearestSpot(unit->GetPos(this->circuit->GetLastFrame()));
		if (mex.index != -1) {
			mex.unitId = unit->GetId();
			*d_first++ = mex;
			clusterInfos[metalInfos[mex.index].clusterId].finishedCount++;
			SetOpenSpot(mex.index, false);  // circuit->IsAllyAware()?
		}
	};
	auto delMex = [this](const SMex& mex) {
		clusterInfos[metalInfos[mex.index].clusterId].finishedCount--;
		SetOpenSpot(mex.index, true);  // circuit->IsAllyAware()?
	};

	// @see std::set_symmetric_difference + std::set_intersection
	while (first1 != last1) {
		if (first2 == last2) {
			addMex(*first1);  // everything else in first1..last1 is new units
			while (++first1 != last1) {
				addMex(*first1);
			}
			break;
		}

		if ((*first1)->GetId() < first2->unitId) {
			addMex(*first1);  // new unit
			++first1;  // advance mexes
		} else {
			if (first2->unitId < (*first1)->GetId()) {
				delMex(*first2);  // dead unit
			} else {
				*d_first++ = *first2;  // old unit
				++first1;  // advance mexes
			}
			++first2;  // advance prevUnits
		}
	}
	while (first2 != last2) {  // everything else in first2..last2 is dead units
		delMex(*first2++);
	}
}

bool CMetalManager::IsMexInFinished(int index) const
{
	// NOTE: finishedCount updated on lazy MarkAllyMexes call, thus can be invalid
	int idx = metalInfos[index].clusterId;
	return clusterInfos[idx].finishedCount >= GetClusters()[idx].idxSpots.size();
}

int CMetalManager::GetSpotToBuild(const AIFloat3& pos, CMetalData::PointPredicate& predicate)
{
	BuildCluster goal(this, predicate, indices);
	return GetSpotToDo(pos, predicate, [&goal](ShortPath* shortPath) {
		return shortPath->start(goal);
	});
}

int CMetalManager::GetSpotToUpgrade(const AIFloat3& pos, CMetalData::PointPredicate& predicate)
{
	UpgradeCluster goal(this, predicate, indices);
	return GetSpotToDo(pos, predicate, [&goal](ShortPath* shortPath) {
		return shortPath->start(goal);
	});
}

bool CMetalManager::IsSpotValid(int index, const AIFloat3& pos) const
{
	if ((index < 0) || ((size_t)index >= GetSpots().size())) {
		return false;
	}
	return utils::is_equal_pos(GetSpots()[index].position, pos);
}

int CMetalManager::GetSpotToDo(const AIFloat3& pos, CMetalData::PointPredicate& predicate,
		std::function<CMetalData::ClusterGraph::Node (ShortPath* shortPath)>&& doGoal)
{
	int index = FindNearestCluster(pos);
	if (index < 0 || !(*threatFilter)[index]) {
		return -1;
	}
	MarkAllyMexes();

	shortPath->init();
	shortPath->addSource(filteredGraph->nodeFromId(index));
	CMetalData::ClusterGraph::Node target = doGoal(shortPath);

	int result = -1;
	if (target != lemon::INVALID) {
		float sqMinDist = std::numeric_limits<float>::max();
		for (int index : indices) {
			float sqDist = GetSpots()[index].position.SqDistance2D(pos);
			if (sqDist < sqMinDist) {
				sqMinDist = sqDist;
				result = index;
			}
		}
	}
	indices.clear();
	return result;
}

} // namespace circuit
