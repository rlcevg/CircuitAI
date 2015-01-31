/*
 * MetalManager.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "static/MetalManager.h"
#include "CircuitAI.h"
#include "util/RagMatrix.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#include "json/json.h"

#include "Game.h"
#include "GameRulesParam.h"
#include "UnitDef.h"

namespace circuit {

using namespace springai;

CMetalManager::CMetalManager(CCircuitAI* circuit, CMetalData* metalData) :
		circuit(circuit),
		metalData(metalData)
{
	if (!metalData->IsInitialized()) {
		// TODO: Add metal zone and no-metal-spots maps support
		std::vector<GameRulesParam*> gameRulesParams = circuit->GetGame()->GetGameRulesParams();
		ParseMetalSpots(gameRulesParams);
		utils::free_clear(gameRulesParams);
	}
	MetalInfo mi = {true};
	metalInfos.resize(metalData->GetSpots().size(), mi);
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

	std::vector<CMetalData::Metal> spots;
	spots.reserve(root.size());
	for (const Json::Value& object : root) {
		CMetalData::Metal spot;
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

	std::vector<CMetalData::Metal> spots(mexCount);
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
	UnitDef* def = circuit->GetUnitDefByName("armestor");
	const std::map<std::string, std::string>& customParams = def->GetCustomParams();
	auto search = customParams.find("pylonrange");
	float radius = (search != customParams.end()) ? utils::string_to_float(search->second) : 500;
	float maxDistance = radius * 2;

	const CMetalData::Metals& spots = metalData->GetSpots();
	int nrows = spots.size();

	std::shared_ptr<CRagMatrix> pdistmatrix = std::make_shared<CRagMatrix>(nrows);
	CRagMatrix& distmatrix = *pdistmatrix;
	for (int i = 1; i < nrows; i++) {
		for (int j = 0; j < i; j++) {
			distmatrix(i, j) = spots[i].position.distance2D(spots[j].position);
		}
	}

	circuit->GetScheduler()->RunParallelTask(std::make_shared<CGameTask>(&CMetalData::Clusterize, metalData, maxDistance, pdistmatrix));
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

void CMetalManager::SetOpenSpot(int index, bool value)
{
	metalInfos[index].isOpen = value;
}

void CMetalManager::SetOpenSpot(const springai::AIFloat3& pos, bool value)
{
	int index = FindNearestSpot(pos);
	if (index != -1) {
		SetOpenSpot(index, true);
	}
}

const std::vector<CMetalManager::MetalInfo>& CMetalManager::GetMetalInfos() const
{
	return metalInfos;
}

} // namespace circuit
