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
#include "EconomyManager.h"
#include "MilitaryManager.h"
#include "CircuitUnit.h"
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
#include "GameRulesParam.h"
#include "SkirmishAI.h"
#include "WrappUnit.h"
//#include "Cheats.h"

namespace circuit {

using namespace springai;

std::unique_ptr<CGameAttribute> CCircuit::gameAttribute(nullptr);
unsigned int CCircuit::gaCounter = 0;

CCircuit::CCircuit(OOAICallback* callback) :
		initialized(false),
		lastFrame(0),
		callback(callback),
		log(std::unique_ptr<Log>(callback->GetLog())),
		game(std::unique_ptr<Game>(callback->GetGame())),
		map(std::unique_ptr<Map>(callback->GetMap())),
		pathing(std::unique_ptr<Pathing>(callback->GetPathing())),
		drawer(std::unique_ptr<Drawer>(map->GetDrawer())),
		skirmishAI(std::unique_ptr<SkirmishAI>(callback->GetSkirmishAI())),
		skirmishAIId(-1)
{
	teamId = skirmishAI->GetTeamId();
	allyTeamId = game->GetMyAllyTeam();
}

CCircuit::~CCircuit()
{
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
	if (!gameAttribute->HasMetalSpots(false)) {
		// TODO: Add metal zone maps support
		std::vector<GameRulesParam*> gameRulesParams = game->GetGameRulesParams();
		gameAttribute->ParseMetalSpots(gameRulesParams);
		utils::FreeClear(gameRulesParams);
	}
	if (!gameAttribute->HasUnitDefs()) {
		std::vector<UnitDef*> unitDefs = callback->GetUnitDefs();
		gameAttribute->InitUnitDefs(unitDefs);
	}

	bool canChooseStartPos = gameAttribute->HasStartBoxes() && gameAttribute->CanChooseStartPos();
	if (gameAttribute->HasMetalSpots()) {
		if (!gameAttribute->HasMetalClusters()) {
			ClusterizeMetal();
			scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CCircuit::ClusterizeMetal, this), 180);
		}
		if (canChooseStartPos) {
			// Parallel task is only to ensure its execution after CMetalManager::Clusterize
			scheduler->RunParallelTask(std::make_shared<CGameTask>([this]() {
				gameAttribute->PickStartPos(GetGame(), GetMap(), StartPosType::METAL_SPOT);
			}));
		}
	} else if (canChooseStartPos) {
		gameAttribute->PickStartPos(GetGame(), GetMap(), StartPosType::MIDDLE);
	}

	modules.push_back(std::unique_ptr<CEconomyManager>(new CEconomyManager(this)));
	modules.push_back(std::unique_ptr<CMilitaryManager>(new CMilitaryManager(this)));

//	Cheats* cheats = callback->GetCheats();
//	cheats->SetEnabled(true);
//	cheats->SetEventsEnabled(true);
//	delete cheats;

	initialized = true;

	return 0;  // signaling: OK
}

int CCircuit::Release(int reason)
{
	DestroyGameAttribute();
	scheduler = nullptr;
	modules.clear();
	for (auto& kv : aliveUnits) {
		delete kv.second;
	}

	initialized = false;

	return 0;  // signaling: OK
}

int CCircuit::Update(int frame)
{
	lastFrame = frame;
	scheduler->ProcessTasks(frame);

	return 0;  // signaling: OK
}

int CCircuit::Message(int playerId, const char* message)
{
	size_t msgLength = strlen(message);

	if (msgLength == strlen("~стройсь") && strcmp(message, "~стройсь") == 0) {
		gameAttribute->PickStartPos(GetGame(), GetMap(), StartPosType::RANDOM);
	}

	else if (strncmp(message, "~selfd", 6) == 0) {
		std::vector<Unit*> units = callback->GetTeamUnits();
		units[0]->SelfDestruct();
		utils::FreeClear(units);
	}

	return 0;  // signaling: OK
}

int CCircuit::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
//	if (builder != nullptr) {
//		if (unit->GetUnit()->IsBeingBuilt()) {
//			for (WorkerTask wt : workerTasks) {
//				if(wt instanceof ProductionTask) {
//					ProductionTask ct = (ProductionTask)wt;
//					if (ct.getWorker().getUnit().getUnitId() == builder.getUnitId()) {
//						ct.setBuilding(unit);
//					}
//				}
//			}
//		} else {
//			for (WorkerTask wt : workerTasks){
//				if (wt instanceof ProductionTask) {
//					ProductionTask ct = (ProductionTask)wt;
//					if (ct.getWorker().getUnit().getUnitId() == builder.getUnitId()) {
//						ct.setCompleted();
//					}
//				}
//			}
//		}
//	}
	for (auto& module : modules) {
		module->UnitCreated(unit, builder);
	}

	return 0;  // signaling: OK
}

int CCircuit::UnitFinished(CCircuitUnit* unit)
{
	for (auto& module : modules) {
		module->UnitFinished(unit);
	}

	return 0;  // signaling: OK
}

int CCircuit::LuaMessage(const char* inData)
{
//	if (strncmp(inData, "METAL_SPOTS:", 12) == 0) {
//		gameAttribute->ParseMetalSpots(inData + 12);
//	}
	return 0;  // signaling: OK
}

CCircuitUnit* CCircuit::RegisterUnit(int unitId)
{
	CCircuitUnit* u = GetUnitById(unitId);
	if (u != nullptr) {
		return u;
	}

	springai::Unit* unit = springai::WrappUnit::GetInstance(skirmishAIId, unitId);
	u = new CCircuitUnit(unit);
	aliveUnits[unitId] = u;

	if (unit->GetTeam() == GetTeamId()) {
		teamUnits.push_back(u);
		friendlyUnits.push_back(u);
	} else if (unit->GetAllyTeam() == allyTeamId) {
		friendlyUnits.push_back(u);
	} else {
		enemyUnits.push_back(u);
	}

	return u;
}

CCircuitUnit* CCircuit::GetUnitById(int unitId)
{
	std::map<int, CCircuitUnit*>::iterator i = aliveUnits.find(unitId);
	if (i != aliveUnits.end()) {
		return i->second;
	}

	return nullptr;
}

CGameAttribute* CCircuit::GetGameAttribute()
{
	return gameAttribute.get();
}

CScheduler* CCircuit::GetScheduler()
{
	return scheduler.get();
}

int CCircuit::GetLastFrame()
{
	return lastFrame;
}

int CCircuit::GetSkirmishAIId()
{
	return skirmishAIId;
}

int CCircuit::GetTeamId()
{
	return teamId;
}

int CCircuit::GetAllyTeamId()
{
	return allyTeamId;
}

OOAICallback* CCircuit::GetCallback()
{
	return callback;
}

Log* CCircuit::GetLog()
{
	return log.get();
}

Game* CCircuit::GetGame()
{
	return game.get();
}

Map* CCircuit::GetMap()
{
	return map.get();
}

Pathing* CCircuit::GetPathing()
{
	return pathing.get();
}

Drawer* CCircuit::GetDrawer()
{
	return drawer.get();
}

SkirmishAI* CCircuit::GetSkirmishAI()
{
	return skirmishAI.get();
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
	if (gameAttribute->GetMetalManager().IsClusterizing()) {
		return;
	}

	MoveData* moveData = gameAttribute->GetUnitDefByName("armcom1")->GetMoveData();
	int pathType = moveData->GetPathType();
	delete moveData;
	float distance = gameAttribute->GetUnitDefByName("corrl")->GetMaxWeaponRange();
	gameAttribute->ClusterizeMetal(scheduler, distance * 2, pathType, GetPathing());
}

// debug
void CCircuit::DrawClusters()
{
	gameAttribute->GetMetalManager().DrawConvexHulls(GetDrawer());
//	gameAttribute->GetMetalManager().DrawCentroids(GetDrawer());
}

} // namespace circuit
