/*
 * MetalManager.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "MetalManager.h"
#include "RagMatrix.h"
#include "CircuitAI.h"
#include "Scheduler.h"
#include "utils.h"
#include "json/json.h"

#include "Game.h"
#include "GameRulesParam.h"
#include "Pathing.h"
#include "UnitDef.h"
#include "MoveData.h"

namespace circuit {

using namespace springai;

#define CLUSTER_MS	8

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

void CMetalManager::ClusterizeMetalFirst()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	metalData->SetClusterizing(true);

	// prepare parameters
	MoveData* moveData = circuit->GetUnitDefByName("armcom1")->GetMoveData();
	int pathType = moveData->GetPathType();
	delete moveData;
	UnitDef* def = circuit->GetUnitDefByName("armestor");
	std::map<std::string, std::string> customParams = def->GetCustomParams();
	float radius = utils::string_to_float(customParams["pylonrange"]);
	float maxDistance = radius * 2;
	Pathing* pathing = circuit->GetPathing();

	const CMetalData::Metals& spots = metalData->GetSpots();
	int nrows = spots.size();

	std::shared_ptr<CRagMatrix> pdistmatrix = std::make_shared<CRagMatrix>(nrows);
	CRagMatrix& distmatrix = *pdistmatrix;
	for (int i = 1; i < nrows; i++) {
		for (int j = 0; j < i; j++) {
			float lenStartEnd = pathing->GetApproximateLength(spots[i].position, spots[j].position, pathType, 0.0f);
			float lenEndStart = pathing->GetApproximateLength(spots[j].position, spots[i].position, pathType, 0.0f);
			distmatrix(i, j) = (lenStartEnd + lenEndStart) / 2.0f;
		}
	}

	circuit->GetScheduler()->RunParallelTask(std::make_shared<CGameTask>(&CMetalData::Clusterize, metalData, maxDistance, pdistmatrix));
}

void CMetalManager::ClusterizeMetal(std::shared_ptr<CScheduler> scheduler)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	metalData->SetClusterizing(true);

	// prepare parameters
	MoveData* moveData = circuit->GetUnitDefByName("armcom1")->GetMoveData();
	int pathType = moveData->GetPathType();
	delete moveData;
	UnitDef* def = circuit->GetUnitDefByName("armestor");
	std::map<std::string, std::string> customParams = def->GetCustomParams();
	float radius = utils::string_to_float(customParams["pylonrange"]);
	float maxDistance = radius * 2;
	Pathing* pathing = circuit->GetPathing();

	const CMetalData::Metals& spots = metalData->GetSpots();

	tmpDistStruct.i = 1;
	tmpDistStruct.matrix = std::make_shared<CRagMatrix>(spots.size());
	tmpDistStruct.maxDistance = maxDistance;
	tmpDistStruct.pathType = pathType;
	tmpDistStruct.pathing = pathing;
	tmpDistStruct.schedWeak = scheduler;
	tmpDistStruct.task = std::make_shared<CGameTask>(&CMetalManager::FillDistMatrix, this);

	scheduler->RunTaskEvery(tmpDistStruct.task, 1);
}

void CMetalManager::FillDistMatrix()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	const CMetalData::Metals& spots = metalData->GetSpots();
	CRagMatrix& distmatrix = *tmpDistStruct.matrix;
	Pathing* pathing = tmpDistStruct.pathing;
	int pathType = tmpDistStruct.pathType;
	int nrows = distmatrix.GetNrows();

	using clock = std::chrono::high_resolution_clock;
	using std::chrono::milliseconds;
	clock::time_point t0 = clock::now();

	for (int i = tmpDistStruct.i; i < nrows; i++) {
		for (int j = 0; j < i; j++) {
			float lenStartEnd = pathing->GetApproximateLength(spots[i].position, spots[j].position, pathType, 0.0f);
			float lenEndStart = pathing->GetApproximateLength(spots[j].position, spots[i].position, pathType, 0.0f);
			distmatrix(i, j) = (lenStartEnd + lenEndStart) / 2.0f;
		}

		clock::time_point t1 = clock::now();
		if (std::chrono::duration_cast<milliseconds>(t1 - t0) > milliseconds(CLUSTER_MS)) {
			tmpDistStruct.i = i + 1;
			return;
		}
	}

	metalData->SetDistMatrix(distmatrix);
	std::shared_ptr<CScheduler> scheduler = tmpDistStruct.schedWeak.lock();
	if (scheduler != nullptr) {
		scheduler->RunParallelTask(std::make_shared<CGameTask>(&CMetalData::Clusterize, metalData, tmpDistStruct.maxDistance, tmpDistStruct.matrix));
		scheduler->RemoveTask(tmpDistStruct.task);
	}
//	tmpDistStruct.schedWeak = nullptr;
	tmpDistStruct.task = nullptr;
}

const CMetalData::Metals& CMetalManager::GetSpots() const
{
	return metalData->GetSpots();
}

const int CMetalManager::FindNearestSpot(const springai::AIFloat3& pos) const
{
	return metalData->FindNearestSpot(pos);
}

const int CMetalManager::FindNearestSpot(const springai::AIFloat3& pos, CMetalData::MetalPredicate& predicate) const
{
	return metalData->FindNearestSpot(pos, predicate);
}

const CMetalData::MetalIndices CMetalManager::FindNearestSpots(const springai::AIFloat3& pos, int num) const
{
	return metalData->FindNearestSpots(pos, num);
}

const CMetalData::MetalIndices CMetalManager::FindNearestSpots(const springai::AIFloat3& pos, int num, CMetalData::MetalPredicate& predicate) const
{
	return metalData->FindNearestSpots(pos, num, predicate);
}

const CMetalData::MetalIndices CMetalManager::FindWithinDistanceSpots(const springai::AIFloat3& pos, float maxDistance) const
{
	return metalData->FindWithinDistanceSpots(pos, maxDistance);
}

const CMetalData::MetalIndices CMetalManager::FindWithinRangeSpots(const springai::AIFloat3& posFrom, const springai::AIFloat3& posTo) const
{
	return metalData->FindWithinRangeSpots(posFrom, posTo);
}

const int CMetalManager::FindNearestCluster(const springai::AIFloat3& pos) const
{
	return metalData->FindNearestCluster(pos);
}

const int CMetalManager::FindNearestCluster(const springai::AIFloat3& pos, CMetalData::MetalPredicate& predicate) const
{
	return metalData->FindNearestCluster(pos, predicate);
}

const CMetalData::MetalIndices CMetalManager::FindNearestClusters(const springai::AIFloat3& pos, int num, CMetalData::MetalPredicate& predicate) const
{
	return metalData->FindNearestClusters(pos, num, predicate);
}

void CMetalManager::ClusterLock()
{
	metalData->ClusterLock();
}

void CMetalManager::ClusterUnlock()
{
	metalData->ClusterUnlock();
}

const std::vector<CMetalData::MetalIndices>& CMetalManager::GetClusters() const
{
	return metalData->GetClusters();
}

const std::vector<springai::AIFloat3>& CMetalManager::GetCentroids() const
{
	return metalData->GetCentroids();
}

const std::vector<springai::AIFloat3>& CMetalManager::GetCostCentroids() const
{
	return metalData->GetCostCentroids();
}

} // namespace circuit
