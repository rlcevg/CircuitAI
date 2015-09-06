/*
 * ResourceManager.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "resource/MetalManager.h"
#include "module/EconomyManager.h"
#include "CircuitAI.h"
#include "util/math/RagMatrix.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#include "json/json.h"

#include "Game.h"
#include "GameRulesParam.h"
#include "MoveData.h"
#include "Pathing.h"

namespace circuit {

using namespace springai;

CMetalManager::CMetalManager(CCircuitAI* circuit, CMetalData* metalData)
		: circuit(circuit)
		, metalData(metalData)
		, markFrame(-1)
{
	if (!metalData->IsInitialized()) {
		// TODO: Add metal zone and no-metal-spots maps support
		std::vector<GameRulesParam*> gameRulesParams = circuit->GetGame()->GetGameRulesParams();
		ParseMetalSpots(gameRulesParams);
		utils::free_clear(gameRulesParams);
	}
	metalInfos.resize(metalData->GetSpots().size(), {true, -1});
}

CMetalManager::~CMetalManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CMetalManager::ParseMetalSpots(const char* metalJson)
{
	Json::Value root;
	Json::Reader json;

	if (!json.parse(metalJson, root, false)) {
		return;
	}

	std::vector<CMetalData::SMetal> spots;
	spots.reserve(root.size());
	for (const Json::Value& object : root) {
		CMetalData::SMetal spot;
		spot.income = object["metal"].asFloat();
		spot.position = AIFloat3(object["x"].asFloat(),
								 object["y"].asFloat(),
								 object["z"].asFloat());
		spots.push_back(spot);
	}

	metalData->Init(spots);
}

void CMetalManager::ParseMetalSpots(const std::vector<GameRulesParam*>& gameParams)
{
	int mexCount = 0;
	for (auto param : gameParams) {
		if (strcmp(param->GetName(), "mex_count") == 0) {
			mexCount = param->GetValueFloat();
			break;
		}
	}

	if (mexCount <= 0) {
		return;
	}

	std::vector<CMetalData::SMetal> spots(mexCount);
	int i = 0;
	for (auto param : gameParams) {
		const char* name = param->GetName();
		if (strncmp(name, "mex_", 4) == 0) {
			if (strncmp(name + 4, "x", 1) == 0) {
				int idx = std::atoi(name + 5);
				spots[idx - 1].position.x = param->GetValueFloat();
				i++;
			} else if (strncmp(name + 4, "y", 1) == 0) {
				int idx = std::atoi(name + 5);
				spots[idx - 1].position.y = param->GetValueFloat();
				i++;
			} else if (strncmp(name + 4, "z", 1) == 0) {
				int idx = std::atoi(name + 5);
				spots[idx - 1].position.z = param->GetValueFloat();
				i++;
			} else if (strncmp(name + 4, "metal", 5) == 0) {
				int idx = std::atoi(name + 9);
				spots[idx - 1].income = param->GetValueFloat();
				i++;
			}

			if (i >= mexCount * 4) {
				break;
			}
		}
	}

	metalData->Init(spots);
}

bool CMetalManager::HasMetalSpots()
{
	return (metalData->IsInitialized() && !metalData->IsEmpty());
}

bool CMetalManager::HasMetalClusters()
{
	return !metalData->GetClusters().empty();
}

bool CMetalManager::IsClusterizing()
{
	return metalData->IsClusterizing();
}

void CMetalManager::ClusterizeMetal()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	metalData->SetClusterizing(true);

	// prepare parameters
	MoveData* moveData = circuit->GetCircuitDef("armrectr")->GetUnitDef()->GetMoveData();
	int pathType = moveData->GetPathType();
	delete moveData;
	UnitDef* def = circuit->GetCircuitDef("armestor")->GetUnitDef();
	const std::map<std::string, std::string>& customParams = def->GetCustomParams();
	auto search = customParams.find("pylonrange");
	float radius = (search != customParams.end()) ? utils::string_to_float(search->second) : PYLON_RANGE;
	float maxDistance = radius * 2;
	Pathing* pathing = circuit->GetPathing();

	const CMetalData::Metals& spots = metalData->GetSpots();
	int nrows = spots.size();

	std::shared_ptr<CRagMatrix> pdistmatrix = std::make_shared<CRagMatrix>(nrows);
	CRagMatrix& distmatrix = *pdistmatrix;
	for (int i = 1; i < nrows; i++) {
		for (int j = 0; j < i; j++) {
			float pathLength = pathing->GetApproximateLength(spots[i].position, spots[j].position, pathType, 0.0f);
			float geomLength = spots[i].position.distance2D(spots[j].position);
			distmatrix(i, j) = (geomLength * 1.4f < pathLength) ? pathLength : geomLength;
		}
	}

	// TODO: Remove parallelism (and fix Init everywhere)
	circuit->GetScheduler()->RunParallelTask(std::make_shared<CGameTask>(&CMetalData::Clusterize, metalData, maxDistance, pdistmatrix));
}

void CMetalManager::Init()
{
	clusterInfos.resize(GetClusters().size(), {0});
	for (int i = 0; i < clusterInfos.size(); ++i) {
		for (int idx : GetClusters()[i].idxSpots) {
			metalInfos[idx].clusterId = i;
		}
	}
}

const CMetalData::Metals& CMetalManager::GetSpots() const
{
	return metalData->GetSpots();
}

const int CMetalManager::FindNearestSpot(const AIFloat3& pos) const
{
	return metalData->FindNearestSpot(pos);
}

const int CMetalManager::FindNearestSpot(const AIFloat3& pos, CMetalData::MetalPredicate& predicate) const
{
	return metalData->FindNearestSpot(pos, predicate);
}

const CMetalData::MetalIndices CMetalManager::FindNearestSpots(const AIFloat3& pos, int num) const
{
	return metalData->FindNearestSpots(pos, num);
}

const CMetalData::MetalIndices CMetalManager::FindNearestSpots(const AIFloat3& pos, int num, CMetalData::MetalPredicate& predicate) const
{
	return metalData->FindNearestSpots(pos, num, predicate);
}

const CMetalData::MetalIndices CMetalManager::FindWithinDistanceSpots(const AIFloat3& pos, float maxDistance) const
{
	return metalData->FindWithinDistanceSpots(pos, maxDistance);
}

const CMetalData::MetalIndices CMetalManager::FindWithinRangeSpots(const AIFloat3& posFrom, const AIFloat3& posTo) const
{
	return metalData->FindWithinRangeSpots(posFrom, posTo);
}

const int CMetalManager::FindNearestCluster(const AIFloat3& pos) const
{
	return metalData->FindNearestCluster(pos);
}

const int CMetalManager::FindNearestCluster(const AIFloat3& pos, CMetalData::MetalPredicate& predicate) const
{
	return metalData->FindNearestCluster(pos, predicate);
}

const CMetalData::MetalIndices CMetalManager::FindNearestClusters(const AIFloat3& pos, int num) const
{
	return metalData->FindNearestClusters(pos, num);
}

const CMetalData::MetalIndices CMetalManager::FindNearestClusters(const AIFloat3& pos, int num, CMetalData::MetalPredicate& predicate) const
{
	return metalData->FindNearestClusters(pos, num, predicate);
}

const CMetalData::Clusters& CMetalManager::GetClusters() const
{
	return metalData->GetClusters();
}

const CMetalData::Graph& CMetalManager::GetGraph() const
{
	return metalData->GetGraph();
}

void CMetalManager::SetOpenSpot(int index, bool value)
{
	metalInfos[index].isOpen = value;
}

void CMetalManager::SetOpenSpot(const springai::AIFloat3& pos, bool value)
{
	int index = FindNearestSpot(pos);
	if (index != -1) {
		SetOpenSpot(index, value);
	}
}

bool CMetalManager::IsOpenSpot(int index)
{
	return metalInfos[index].isOpen;
}

void CMetalManager::MarkAllyMexes()
{
	if (markFrame /*+ FRAMES_PER_SEC*/ >= circuit->GetLastFrame()) {
		return;
	}

	circuit->UpdateFriendlyUnits();
	CCircuitDef* mexDef = circuit->GetEconomyManager()->GetMexDef();
	std::list<CCircuitUnit*> mexes, pylons;
	const CAllyTeam::Units& friendlies = circuit->GetFriendlyUnits();
	for (auto& kv : friendlies) {
		CCircuitUnit* unit = kv.second;
		if (*unit->GetCircuitDef() == *mexDef) {
			mexes.push_back(unit);
		}
	}

	MarkAllyMexes(mexes);
}

void CMetalManager::MarkAllyMexes(const std::list<CCircuitUnit*>& mexes)
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
	auto addMex = [&d_first, this](const CCircuitUnit* unit) {
		SMex mex;
		mex.unitId = unit->GetId();
		mex.pos = unit->GetUnit()->GetPos();
		*d_first++ = mex;
		int index = FindNearestSpot(mex.pos);
		if (index != -1) {
			clusterInfos[metalInfos[index].clusterId].mexCount++;
		}
	};
	auto delMex = [this](const SMex& mex) {
		int index = FindNearestSpot(mex.pos);
		if (index != -1) {
			clusterInfos[metalInfos[index].clusterId].mexCount--;
		}
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

bool CMetalManager::IsClusterOur(int index)
{
	return clusterInfos[index].mexCount >= GetClusters()[index].idxSpots.size();
}

} // namespace circuit
