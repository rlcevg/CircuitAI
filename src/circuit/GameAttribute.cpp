/*
 * GameAttribute.cpp
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#include "GameAttribute.h"
#include "SetupManager.h"
#include "MetalManager.h"
#include "Scheduler.h"
#include "utils.h"
#include "json/json.h"

#include "GameRulesParam.h"
#include "Game.h"
#include "Map.h"
#include "UnitDef.h"
#include "Pathing.h"

#include <map>
#include <regex>

namespace circuit {

using namespace springai;

#define CLUSTER_MS	10

CGameAttribute::CGameAttribute() :
		setupManager(nullptr),
		metalManager(nullptr)
{
	srand(time(nullptr));
}

CGameAttribute::~CGameAttribute()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (auto& kv : definitions) {
		delete kv.second;
	}
}

void CGameAttribute::ParseSetupScript(const char* setupScript, int width, int height)
{
	std::map<int, CSetupManager::Box> boxesMap;
	std::regex patternAlly("\\[allyteam(\\d+)\\]\\s*\\{([^\\}]*)\\}");
	std::regex patternRect("startrect\\w+=(\\d+(\\.\\d+)?);");
	std::string script(setupScript);

	std::smatch allyteam;
	std::string::const_iterator start = script.begin();
	std::string::const_iterator end = script.end();
	while (std::regex_search(start, end, allyteam, patternAlly)) {
		int allyTeamId = utils::string_to_int(allyteam[1]);

		std::string teamDefBody = allyteam[2];
		std::sregex_token_iterator iter(teamDefBody.begin(), teamDefBody.end(), patternRect, 1);
		std::sregex_token_iterator end;
		CSetupManager::Box startbox;
		for (int i = 0; iter != end && i < 4; ++iter, i++) {
			startbox.edge[i] = utils::string_to_float(*iter);
		}

		float mapWidth = SQUARE_SIZE * width;
		float mapHeight = SQUARE_SIZE * height;
		startbox.bottom *= mapHeight;
		startbox.left   *= mapWidth;
		startbox.right  *= mapWidth;
		startbox.top    *= mapHeight;
		boxesMap[allyTeamId] = startbox;

		start = allyteam[0].second;
	}

	CGameSetup::StartPosType startPosType;
	std::cmatch matchPosType;
	std::regex patternPosType("startpostype=(\\d+)");
	if (std::regex_search(setupScript, matchPosType, patternPosType)) {
		startPosType = static_cast<CGameSetup::StartPosType>(std::atoi(matchPosType[1].first));
	} else {
		startPosType = CGameSetup::StartPosType::StartPos_ChooseInGame;
	}

	std::vector<CSetupManager::Box> startBoxes;
	// Remap start boxes
	// @see rts/Game/GameSetup.cpp CGameSetup::Init
//	for (const std::map<int, Box>::value_type& kv : boxesMap) {
//	for (const std::pair<const int, std::array<float, 4>>& kv : boxesMap) {
	for (const auto& kv : boxesMap) {
		startBoxes.push_back(kv.second);
	}

	setupManager = std::make_shared<CSetupManager>(startBoxes, startPosType);
}

bool CGameAttribute::HasStartBoxes(bool checkEmpty)
{
	if (checkEmpty) {
		return (setupManager != nullptr && !setupManager->IsEmpty());
	}
	return setupManager != nullptr;
}

bool CGameAttribute::CanChooseStartPos()
{
	return setupManager->CanChooseStartPos();
}

void CGameAttribute::PickStartPos(Game* game, Map* map, StartPosType type)
{
	float x, z;
	const CSetupManager::Box& box = GetSetupManager()[game->GetMyAllyTeam()];

	auto random = [](const CSetupManager::Box& box, float& x, float& z) {
		int min, max;
		min = box.left;
		max = box.right;
		x = min + (rand() % (int)(max - min + 1));
		min = box.top;
		max = box.bottom;
		z = min + (rand() % (int)(max - min + 1));
	};

	switch (type) {
		case StartPosType::METAL_SPOT: {
			AIFloat3 posFrom(box.left, 0, box.top);
			AIFloat3 posTo(box.right, 0, box.bottom);
			CMetalManager::Metals inBoxSpots = metalManager->FindWithinRangeSpots(posFrom, posTo);
			if (!inBoxSpots.empty()) {
				AIFloat3& pos = inBoxSpots[rand() % inBoxSpots.size()].position;
				x = pos.x;
				z = pos.z;
			} else {
				random(box, x, z);
			}
			break;
		}
		case StartPosType::MIDDLE: {
			x = (box.left + box.right) / 2;
			z = (box.top + box.bottom) / 2;
			break;
		}
		case StartPosType::RANDOM:
		default: {
			random(box, x, z);
			break;
		}
	}

	game->SendStartPosition(false, AIFloat3(x, map->GetElevationAt(x, z), z));
}

CSetupManager& CGameAttribute::GetSetupManager()
{
	return *setupManager;
}

void CGameAttribute::ParseMetalSpots(const char* metalJson)
{
	Json::Value root;
	Json::Reader json;

	if (!json.parse(metalJson, root, false)) {
		return;
	}

	int numAllyTeams = setupManager->GetNumAllyTeams();
	std::vector<CMetalManager::Metal> spots;
	for (const Json::Value& object : root) {
		CMetalManager::Metal spot;
		spot.income = object["metal"].asFloat();
		spot.position = AIFloat3(object["x"].asFloat(),
								 object["y"].asFloat(),
								 object["z"].asFloat());
		for (int i = 0; i < numAllyTeams; i++) {
			spot.isOpen.push_back(true);
		}
		spots.push_back(spot);
	}

	metalManager = std::make_shared<CMetalManager>(spots);
}

void CGameAttribute::ParseMetalSpots(const std::vector<GameRulesParam*>& gameParams)
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

	std::vector<CMetalManager::Metal> spots(mexCount);
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

	int numAllyTeams = setupManager->GetNumAllyTeams();
	for (int i = 0; i < mexCount; i++) {
		for (int j = 0; j < numAllyTeams; j++) {
			spots[i].isOpen.push_back(true);
		}
	}

	metalManager = std::make_shared<CMetalManager>(spots);
}

bool CGameAttribute::HasMetalSpots(bool checkEmpty)
{
	if (checkEmpty) {
		return (metalManager != nullptr && !metalManager->IsEmpty());
	}
	return metalManager != nullptr;
}

bool CGameAttribute::HasMetalClusters()
{
	return !metalManager->GetClusters().empty();
}

void CGameAttribute::ClusterizeMetalFirst(std::shared_ptr<CScheduler> scheduler, float maxDistance, int pathType, Pathing* pathing)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	metalManager->SetClusterizing(true);
	const CMetalManager::Metals& spots = metalManager->GetSpots();
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

	scheduler->RunParallelTask(std::make_shared<CGameTask>(&CMetalManager::Clusterize, metalManager, maxDistance, pdistmatrix));
}

void CGameAttribute::ClusterizeMetal(std::shared_ptr<CScheduler> scheduler, float maxDistance, int pathType, Pathing* pathing)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	metalManager->SetClusterizing(true);
	const CMetalManager::Metals& spots = metalManager->GetSpots();

	tmpDistStruct.i = 1;
	tmpDistStruct.matrix = std::make_shared<CRagMatrix>(spots.size());
	tmpDistStruct.maxDistance = maxDistance;
	tmpDistStruct.pathType = pathType;
	tmpDistStruct.pathing = pathing;
	tmpDistStruct.schedWeak = scheduler;
	tmpDistStruct.task = std::make_shared<CGameTask>(&CGameAttribute::FillDistMatrix, this);

	scheduler->RunTaskEvery(tmpDistStruct.task, 1);
}

CMetalManager& CGameAttribute::GetMetalManager()
{
	return *metalManager;
}

void CGameAttribute::InitUnitDefs(std::vector<UnitDef*>&& unitDefs)
{
	if (!definitions.empty()) {
		for (auto& kv : definitions) {
			delete kv.second;
		}
		definitions.clear();
	}
	for (auto def : unitDefs) {
		definitions[def->GetName()] = def;
	}
}

bool CGameAttribute::HasUnitDefs()
{
	return !definitions.empty();
}

UnitDef* CGameAttribute::GetUnitDefByName(const char* name)
{
	decltype(definitions)::iterator i = definitions.find(name);
	if (i != definitions.end()) {
		return i->second;
	}

	return nullptr;
}

CGameAttribute::UnitDefs& CGameAttribute::GetUnitDefs()
{
	return definitions;
}

void CGameAttribute::FillDistMatrix()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	const CMetalManager::Metals& spots = metalManager->GetSpots();
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

	metalManager->SetDistMatrix(distmatrix);
	std::shared_ptr<CScheduler> scheduler = tmpDistStruct.schedWeak.lock();
	if (scheduler != nullptr) {
		scheduler->RunParallelTask(std::make_shared<CGameTask>(&CMetalManager::Clusterize, metalManager, tmpDistStruct.maxDistance, tmpDistStruct.matrix));
		scheduler->RemoveTask(tmpDistStruct.task);
	}
//	tmpDistStruct.schedWeak = nullptr;
	tmpDistStruct.task = nullptr;
}

} // namespace circuit
