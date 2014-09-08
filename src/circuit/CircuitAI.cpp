/*
 * Circuit.cpp
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#include "CircuitAI.h"
#include "GameAttribute.h"
#include "SetupManager.h"
#include "MetalManager.h"
#include "Scheduler.h"
#include "EconomyManager.h"
#include "MilitaryManager.h"
#include "CircuitUnit.h"
#include "utils.h"

#include "ExternalAI/Interface/AISEvents.h"
#include "ExternalAI/Interface/AISCommands.h"
#include "OOAICallback.h"			// C++ wrapper
#include "SSkirmishAICallback.h"	// "direct" C API
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

//#define DEBUG
//#if 1
#ifdef DEBUG
	#define PRINT_TOPIC(txt, topic)	\
		LOG("<CircuitAI> %s topic: %i, SkirmishAIId: %i", txt, topic, skirmishAIId)
//		std::string msgText = std::string("<CircuitAI> ") + txt + " topic: " + utils::int_to_string(topic) + ", SkirmishAIId: " + utils::int_to_string(callback->GetSkirmishAIId());	\
//		springai::Log* log = callback->GetLog();	\
//		log->DoLog(msgText.c_str());	\
//		delete log
#else
	#define PRINT_TOPIC(txt, topic)
#endif
#define DURATION_1_MIN	(30 * 60)

std::unique_ptr<CGameAttribute> CCircuitAI::gameAttribute(nullptr);
unsigned int CCircuitAI::gaCounter = 0;

CCircuitAI::CCircuitAI(OOAICallback* callback) :
		initialized(false),
		lastFrame(0),
		callback(callback),
		log(std::unique_ptr<Log>(callback->GetLog())),
		game(std::unique_ptr<Game>(callback->GetGame())),
		map(std::unique_ptr<Map>(callback->GetMap())),
		pathing(std::unique_ptr<Pathing>(callback->GetPathing())),
		drawer(std::unique_ptr<Drawer>(map->GetDrawer())),
		skirmishAI(std::unique_ptr<SkirmishAI>(callback->GetSkirmishAI())),
		skirmishAIId(callback != NULL ? callback->GetSkirmishAIId() : -1)
{
	teamId = skirmishAI->GetTeamId();
	allyTeamId = game->GetMyAllyTeam();
}

CCircuitAI::~CCircuitAI()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	if (initialized) {
		Release(0);
	}
}

int CCircuitAI::HandleEvent(int topic, const void* data)
{
	int ret = ERROR_UNKNOWN;

	switch (topic) {
		case EVENT_INIT: {
			PRINT_TOPIC("EVENT_INIT", topic);
			struct SInitEvent* evt = (struct SInitEvent*)data;
			ret = this->Init(evt->skirmishAIId, evt->callback);
			break;
		}
		case EVENT_RELEASE: {
			PRINT_TOPIC("EVENT_RELEASE", topic);
			struct SReleaseEvent* evt = (struct SReleaseEvent*)data;
			ret = this->Release(evt->reason);
			break;
		}
		case EVENT_UPDATE: {
//			PRINT_TOPIC("EVENT_UPDATE", topic);
			struct SUpdateEvent* evt = (struct SUpdateEvent*)data;
			ret = this->Update(evt->frame);
			break;
		}
		case EVENT_MESSAGE: {
			PRINT_TOPIC("EVENT_MESSAGE", topic);
			struct SMessageEvent* evt = (struct SMessageEvent*)data;
			ret = this->Message(evt->player, evt->message);;
			break;
		}
		case EVENT_UNIT_CREATED: {
			PRINT_TOPIC("EVENT_UNIT_CREATED", topic);
			struct SUnitCreatedEvent* evt = (struct SUnitCreatedEvent*)data;
			CCircuitUnit* builder = (evt->builder < 0) ? nullptr : GetUnitById(evt->builder);
			CCircuitUnit* unit = RegisterUnit(evt->unit);
			ret = this->UnitCreated(unit, builder);
			break;
		}
		case EVENT_UNIT_FINISHED: {
			PRINT_TOPIC("EVENT_UNIT_FINISHED", topic);
			struct SUnitFinishedEvent* evt = (struct SUnitFinishedEvent*)data;
			// Lua might call SetUnitHealth within eventHandler.UnitCreated(this, builder);
			// and trigger UnitFinished before eoh->UnitCreated(*this, builder);
			// @see rts/Sim/Units/Unit.cpp CUnit::PostInit
			CCircuitUnit* unit = RegisterUnit(evt->unit);
			ret = this->UnitFinished(unit);
			break;
		}
		case EVENT_UNIT_IDLE: {
			PRINT_TOPIC("EVENT_UNIT_IDLE", topic);
			struct SUnitIdleEvent* evt = (struct SUnitIdleEvent*)data;
			CCircuitUnit* unit = GetUnitById(evt->unit);
			ret = this->UnitIdle(unit);
			break;
		}
		case EVENT_UNIT_MOVE_FAILED: {
			PRINT_TOPIC("EVENT_UNIT_MOVE_FAILED", topic);
			ret = 0;
			break;
		}
		case EVENT_UNIT_DAMAGED: {
			PRINT_TOPIC("EVENT_UNIT_DAMAGED", topic);
			ret = 0;
			break;
		}
		case EVENT_UNIT_DESTROYED: {
			PRINT_TOPIC("EVENT_UNIT_DESTROYED", topic);
			struct SUnitDestroyedEvent* evt = (struct SUnitDestroyedEvent*)data;
			CCircuitUnit* attacker = GetUnitById(evt->attacker);
			CCircuitUnit* unit = GetUnitById(evt->unit);
			ret = this->UnitDestroyed(unit, attacker);
			UnregisterUnit(evt->unit);
			break;
		}
		case EVENT_UNIT_GIVEN: {
			PRINT_TOPIC("EVENT_UNIT_GIVEN", topic);
			struct SUnitGivenEvent* evt = (struct SUnitGivenEvent*)data;
			CCircuitUnit* unit = RegisterUnit(evt->unitId);
			ret = this->UnitGiven(unit, evt->oldTeamId, evt->newTeamId);
			break;
		}
		case EVENT_UNIT_CAPTURED: {
			PRINT_TOPIC("EVENT_UNIT_CAPTURED", topic);
			struct SUnitCapturedEvent* evt = (struct SUnitCapturedEvent*)data;
			CCircuitUnit* unit = GetUnitById(evt->unitId);
			ret = this->UnitCaptured(unit, evt->oldTeamId, evt->newTeamId);
			UnregisterUnit(evt->unitId);
			break;
		}
		case EVENT_ENEMY_ENTER_LOS: {
			PRINT_TOPIC("EVENT_ENEMY_ENTER_LOS", topic);
			ret = 0;
			break;
		}
		case EVENT_ENEMY_LEAVE_LOS: {
			PRINT_TOPIC("EVENT_ENEMY_LEAVE_LOS", topic);
			ret = 0;
			break;
		}
		case EVENT_ENEMY_ENTER_RADAR: {
			PRINT_TOPIC("EVENT_ENEMY_ENTER_RADAR", topic);
			ret = 0;
			break;
		}
		case EVENT_ENEMY_LEAVE_RADAR: {
			PRINT_TOPIC("EVENT_ENEMY_LEAVE_RADAR", topic);
			ret = 0;
			break;
		}
		case EVENT_ENEMY_DAMAGED: {
			PRINT_TOPIC("EVENT_ENEMY_DAMAGED", topic);
			ret = 0;
			break;
		}
		case EVENT_ENEMY_DESTROYED: {
			PRINT_TOPIC("EVENT_ENEMY_DESTROYED", topic);
			ret = 0;
			break;
		}
		case EVENT_WEAPON_FIRED: {
			PRINT_TOPIC("EVENT_WEAPON_FIRED", topic);
			ret = 0;
			break;
		}
		case EVENT_PLAYER_COMMAND: {
			PRINT_TOPIC("EVENT_PLAYER_COMMAND", topic);
			ret = 0;
			break;
		}
		case EVENT_SEISMIC_PING: {
			PRINT_TOPIC("EVENT_SEISMIC_PING", topic);
			ret = 0;
			break;
		}
		case EVENT_COMMAND_FINISHED: {
			PRINT_TOPIC("EVENT_COMMAND_FINISHED", topic);
			struct SCommandFinishedEvent* evt = (struct SCommandFinishedEvent*)data;
			ret = 0;
			break;
		}
		case EVENT_LOAD: {
			PRINT_TOPIC("EVENT_LOAD", topic);
			ret = 0;
			break;
		}
		case EVENT_SAVE: {
			PRINT_TOPIC("EVENT_SAVE", topic);
			ret = 0;
			break;
		}
		case EVENT_ENEMY_CREATED: {
			PRINT_TOPIC("EVENT_ENEMY_CREATED", topic);
			// @see Cheats::SetEventsEnabled
			ret = 0;
			break;
		}
		case EVENT_ENEMY_FINISHED: {
			PRINT_TOPIC("EVENT_ENEMY_FINISHED", topic);
			// @see Cheats::SetEventsEnabled
			ret = 0;
			break;
		}
		case EVENT_LUA_MESSAGE: {
			PRINT_TOPIC("EVENT_LUA_MESSAGE", topic);
			struct SLuaMessageEvent* evt = (struct SLuaMessageEvent*)data;
			ret = this->LuaMessage(evt->inData);
			break;
		}
		default: {
			LOG("<CircuitAI> %i WARNING unrecognized event: %i", skirmishAIId, topic);
			ret = 0;
			break;
		}
	}

	return ret;
}

int CCircuitAI::Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback)
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
		if (!gameAttribute->HasMetalClusters() && !gameAttribute->GetMetalManager().IsClusterizing()) {
			MoveData* moveData = gameAttribute->GetUnitDefByName("armcom1")->GetMoveData();
			int pathType = moveData->GetPathType();
			delete moveData;
			float distance = gameAttribute->GetUnitDefByName("corrl")->GetMaxWeaponRange();
			gameAttribute->ClusterizeMetalFirst(scheduler, distance * 2, pathType, GetPathing());

			scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CCircuitAI::ClusterizeMetal, this), DURATION_1_MIN);
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

int CCircuitAI::Release(int reason)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	DestroyGameAttribute();
	scheduler = nullptr;
	modules.clear();
	for (auto& kv : aliveUnits) {
		delete kv.second;
	}

	initialized = false;

	return 0;  // signaling: OK
}

int CCircuitAI::Update(int frame)
{
	lastFrame = frame;

	scheduler->ProcessTasks(frame);

	return 0;  // signaling: OK
}

int CCircuitAI::Message(int playerId, const char* message)
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

int CCircuitAI::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
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

int CCircuitAI::UnitFinished(CCircuitUnit* unit)
{
	for (auto& module : modules) {
		module->UnitFinished(unit);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitIdle(CCircuitUnit* unit)
{
	for (auto& module : modules) {
		module->UnitIdle(unit);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	for (auto& module : modules) {
		module->UnitDestroyed(unit, attacker);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId)
{
	printf("unit: %i, oldTeamId: %i, newTeamId: %i, myTeamId: %i\n", unit->GetUnit()->GetUnitId(), oldTeamId, newTeamId, teamId);
	// it might not have been given to us! Could have been given to another team
	if (teamId == newTeamId) {
		for (auto& module : modules) {
			module->UnitGiven(unit, oldTeamId, newTeamId);
		}
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId)
{
	printf("unit: %i, oldTeamId: %i, newTeamId: %i, myTeamId: %i\n", unit->GetUnit()->GetUnitId(), oldTeamId, newTeamId, teamId);
	for (auto& module : modules) {
		module->UnitCaptured(unit, oldTeamId, newTeamId);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::LuaMessage(const char* inData)
{
//	if (strncmp(inData, "METAL_SPOTS:", 12) == 0) {
//		gameAttribute->ParseMetalSpots(inData + 12);
//	}
	return 0;  // signaling: OK
}

CCircuitUnit* CCircuitAI::GetUnitById(int unitId)
{
	std::map<int, CCircuitUnit*>::iterator i = aliveUnits.find(unitId);
	if (i != aliveUnits.end()) {
		return i->second;
	}

	return nullptr;
}

CCircuitUnit* CCircuitAI::RegisterUnit(int unitId)
{
	CCircuitUnit* u = GetUnitById(unitId);
	if (u != nullptr) {
		return u;
	}

	springai::Unit* unit = springai::WrappUnit::GetInstance(skirmishAIId, unitId);
	UnitDef* unitDef = unit->GetDef();
	u = new CCircuitUnit(unit, gameAttribute->GetUnitDefByName(unitDef->GetName()));
	delete unitDef;
	aliveUnits[unitId] = u;

	if (unit->GetTeam() == GetTeamId()) {
		teamUnits[unitId] = u;
		friendlyUnits[unitId] = u;
	} else if (unit->GetAllyTeam() == allyTeamId) {
		friendlyUnits[unitId] = u;
	} else {
		enemyUnits[unitId] = u;
	}

	return u;
}

void CCircuitAI::UnregisterUnit(int unitId)
{
	CCircuitUnit* u = GetUnitById(unitId);
	if (u == nullptr) {
		return;
	}

	aliveUnits.erase(unitId);

	if (u->GetUnit()->GetTeam() == GetTeamId()) {
		teamUnits.erase(unitId);
		friendlyUnits.erase(unitId);
	} else if (u->GetUnit()->GetAllyTeam() == allyTeamId) {
		friendlyUnits.erase(unitId);
	} else {
		enemyUnits.erase(unitId);
	}

	delete u;
}

CGameAttribute* CCircuitAI::GetGameAttribute()
{
	return gameAttribute.get();
}

CScheduler* CCircuitAI::GetScheduler()
{
	return scheduler.get();
}

int CCircuitAI::GetLastFrame()
{
	return lastFrame;
}

int CCircuitAI::GetSkirmishAIId()
{
	return skirmishAIId;
}

int CCircuitAI::GetTeamId()
{
	return teamId;
}

int CCircuitAI::GetAllyTeamId()
{
	return allyTeamId;
}

OOAICallback* CCircuitAI::GetCallback()
{
	return callback;
}

Log* CCircuitAI::GetLog()
{
	return log.get();
}

Game* CCircuitAI::GetGame()
{
	return game.get();
}

Map* CCircuitAI::GetMap()
{
	return map.get();
}

Pathing* CCircuitAI::GetPathing()
{
	return pathing.get();
}

Drawer* CCircuitAI::GetDrawer()
{
	return drawer.get();
}

SkirmishAI* CCircuitAI::GetSkirmishAI()
{
	return skirmishAI.get();
}

void CCircuitAI::CreateGameAttribute()
{
	if (gameAttribute == nullptr) {
		gameAttribute = std::unique_ptr<CGameAttribute>(new CGameAttribute());
	}
	gaCounter++;
}

void CCircuitAI::DestroyGameAttribute()
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

void CCircuitAI::ClusterizeMetal()
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
void CCircuitAI::DrawClusters()
{
	gameAttribute->GetMetalManager().DrawConvexHulls(GetDrawer());
//	gameAttribute->GetMetalManager().DrawCentroids(GetDrawer());
}

} // namespace circuit
