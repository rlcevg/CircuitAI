/*
 * Circuit.cpp
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#include "Circuit.h"
#include "GameAttribute.h"
#include "SetupManager.h"
#include "MetalManager.h"
#include "Scheduler.h"
#include "GameTask.h"
#include "utils.h"

#include "AIFloat3.h"
#include "Game.h"
#include "Map.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Log.h"
#include "Pathing.h"
#include "MoveData.h"
#include "Drawer.h"

namespace circuit {

using namespace springai;

std::unique_ptr<CGameAttribute> CCircuit::gameAttribute(nullptr);
unsigned int CCircuit::gaCounter = 0;

CCircuit::CCircuit(springai::OOAICallback* callback) :
		initialized(false),
		callback(callback),
		log(callback->GetLog()),
		game(callback->GetGame()),
		map(callback->GetMap()),
		pathing(callback->GetPathing()),
		skirmishAIId(-1)
{
}

CCircuit::~CCircuit()
{
	printf("<DEBUG> Entering:  %s,\t skirmishId: %i\n", __PRETTY_FUNCTION__, skirmishAIId);
	if (initialized) {
		Release(0);
	}
}

int CCircuit::Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback)
{
	this->skirmishAIId = skirmishAIId;
	CreateGameAttribute();
	scheduler = std::make_shared<CScheduler>();
	scheduler->Init(scheduler);

	if (!gameAttribute->HasStartBoxes(false)) {
		gameAttribute->ParseSetupScript(game->GetSetupScript(), map->GetWidth(), map->GetHeight());
	}
	// level 0: Check if GameRulesParams have metal spots
	if (!gameAttribute->HasMetalSpots(false)) {
		// TODO: Add metal zone maps support
		std::vector<springai::GameRulesParam*> gameRulesParams = game->GetGameRulesParams();
		gameAttribute->ParseMetalSpots(gameRulesParams);
	}

	if (gameAttribute->HasStartBoxes()) {
		CSetupManager& setup = gameAttribute->GetSetupManager();

		if (setup.CanChoosePos()) {
			setup.PickStartPos(game, map);
		}
	}

	initialized = true;
	// signal: everything went OK
	return 0;
}

int CCircuit::Release(int reason)
{
	printf("<DEBUG> Entering:  %s,\t skirmishId: %i\n", __PRETTY_FUNCTION__, callback->GetSkirmishAIId());
	DestroyGameAttribute();
	scheduler = nullptr;

	initialized = false;
	// signal: everything went OK
	return 0;
}

int CCircuit::Update(int frame)
{
	if (frame == 120) {
//		LOG("HIT 300 frame");
		std::vector<springai::Unit*> units = callback->GetTeamUnits();
		if (units.size() > 0) {
//			LOG("found mah comm");
			Unit* commander = units.front();
			Unit* friendCommander = NULL;;
			std::vector<springai::Unit*> friendlies = callback->GetFriendlyUnits();
			for (Unit* unit : friendlies) {
				UnitDef* unitDef = unit->GetDef();
				if (strcmp(unitDef->GetName(), "armcom1") == 0) {
					if (commander->GetUnitId() != unit->GetUnitId()) {
//						LOG("found friendly comm");
						friendCommander = unit;
						break;
					} else {
//						LOG("found mah comm again");
					}
				}
			}

			if (friendCommander) {
				LOG("giving guard order");
				commander->Guard(friendCommander);
//				commander->Build(callback->GetUnitDefByName("armsolar"), commander->GetPos(), UNIT_COMMAND_BUILD_NO_FACING);
			}
		}
	}

	scheduler->ProcessTasks(frame);

	// signal: everything went OK
	return 0;
}

int CCircuit::Message(int playerId, const char* message)
{
	size_t msgLength = strlen(message);

	if (msgLength == strlen("~стройсь") && strcmp(message, "~стройсь") == 0) {
		CSetupManager& setup = gameAttribute->GetSetupManager();
		setup.PickStartPos(game, map);
	}

	else if (strncmp(message, "~selfd", 6) == 0) {
		callback->GetTeamUnits()[0]->SelfDestruct();
	}

	else if (callback->GetSkirmishAIId() == 0) {

		if (msgLength == strlen("~кластер") && strcmp(message, "~кластер") == 0) {
			if (gameAttribute->HasMetalSpots()) {
				gameAttribute->GetMetalManager().ClearMetalClusters(map);
				scheduler->RunParallelTask(std::make_shared<CGameTask>(&CCircuit::ClusterizeMetal, this),
										   std::make_shared<CGameTask>(&CCircuit::DrawClusters, this));
			}
//		} else if (msgLength == strlen("~делитель++") && strncmp(message, "~делитель", strlen("~делитель")) == 0) {	// Non ASCII comparison
//			if (gameAttribute->HasMetalSpots()) {
//				int& divider = gameAttribute->GetMetalManager().mexPerClusterAvg;
//				if (strcmp(message + msgLength - 2, "++") == 0) {	// ASCII comparison
//					if (divider < gameAttribute->GetMetalManager().spots.size()) {
//						gameAttribute->GetMetalManager().mexPerClusterAvg++;
//					}
//				} else if (strcmp(message + msgLength - 2, "--") == 0) {
//					if (divider > 1) {
//						divider--;
//					}
//				}
//				std::string msgText = utils::string_format("/Say Allies: <CircuitAI> Cluster divider = %i (avarage mexes per cluster)", divider);
//				game->SendTextMessage(msgText.c_str(), 0);
//			}
		}
	}

	return 0; //signaling: OK
}

int CCircuit::LuaMessage(const char* inData)
{
//	if (strncmp(inData, "METAL_SPOTS:", 12) == 0) {
//		gameAttribute->ParseMetalSpots(inData + 12);
//	}
	return 0; //signaling: OK
}

int CCircuit::GetSkirmishAIId()
{
	return skirmishAIId;
}

OOAICallback* CCircuit::GetCallback()
{
	return callback;
}

Log* CCircuit::GetLog()
{
	return log;
}

Game* CCircuit::GetGame()
{
	return game;
}

Map* CCircuit::GetMap()
{
	return map;
}

Pathing* CCircuit::GetPathing()
{
	return pathing;
}

void CCircuit::CreateGameAttribute()
{
	if (gameAttribute == nullptr) {
		gameAttribute = std::unique_ptr<CGameAttribute>(new CGameAttribute());
	}
	gaCounter++;
}

void CCircuit::DestroyGameAttribute()
{
	if (gaCounter <= 1) {
		if (gameAttribute != nullptr) {
			gameAttribute = nullptr;
			// deletes singleton here;
		}
		gaCounter = 0;
	} else {
		gaCounter--;
	}
}

void CCircuit::ClusterizeMetal()
{
	int pathType = callback->GetUnitDefByName("armcom1")->GetMoveData()->GetPathType();
	float distance = callback->GetUnitDefByName("corrl")->GetMaxWeaponRange();
	gameAttribute->GetMetalManager().Clusterize(distance * 2, pathType, pathing);
}

void CCircuit::DrawClusters()
{
	gameAttribute->GetMetalManager().DrawConvexHulls(map);
//	gameAttribute->GetMetalManager().DrawCentroids(map);
}

} // namespace circuit
