/*
 * Circuit.cpp
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#include "CircuitAI.h"
#include "setup/SetupManager.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "module/EconomyManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "terrain/PathFinder.h"
#include "task/PlayerTask.h"
#include "unit/CircuitUnit.h"
#include "unit/EnemyUnit.h"
#include "util/GameAttribute.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#ifdef DEBUG_VIS
#include "resource/EnergyGrid.h"
#endif

#include "AISEvents.h"
#include "AISCommands.h"
#include "SSkirmishAICallback.h"	// "direct" C API
#include "OOAICallback.h"			// C++ wrapper
#include "Log.h"
#include "Game.h"
#include "Map.h"
#include "Lua.h"
#include "Pathing.h"
#include "Drawer.h"
#include "Economy.h"
#include "Resource.h"
#include "SkirmishAI.h"
#include "WrappUnit.h"
#include "WrappTeam.h"
#include "OptionValues.h"
//#include "Info.h"
//#include "Mod.h"
#include "Cheats.h"
//#include "WrappCurrentCommand.h"

#include <regex>
#include <fstream>

namespace circuit {

using namespace springai;

#define ACTION_UPDATE_RATE	128
#define RELEASE_CONFIG		100
#define RELEASE_COMMANDER	101
#define RELEASE_CORRUPTED	102
#define RELEASE_RESIGN		103
#ifdef DEBUG
	#define PRINT_TOPIC(txt, topic)	LOG("<CircuitAI> %s topic: %i, SkirmishAIId: %i", txt, topic, skirmishAIId)
#else
	#define PRINT_TOPIC(txt, topic)
#endif

std::unique_ptr<CGameAttribute> CCircuitAI::gameAttribute(nullptr);
unsigned int CCircuitAI::gaCounter = 0;

CCircuitAI::CCircuitAI(OOAICallback* callback)
		: eventHandler(&CCircuitAI::HandleGameEvent)
		, economy(nullptr)
		, metalRes(nullptr)
		, energyRes(nullptr)
		, allyTeam(nullptr)
		, uEnemyMark(0)
		, kEnemyMark(0)
		, actionIterator(0)
		, isCheating(false)
		, isAllyAware(true)
		, isCommMerge(true)
		, isInitialized(false)
		, isResigned(false)
		// NOTE: assert(lastFrame != -1): CCircuitUnit initialized with -1
		//       and lastFrame check will misbehave until first update event.
		, lastFrame(0)
		, skirmishAIId(callback != NULL ? callback->GetSkirmishAIId() : -1)
		, sAICallback(nullptr)
		, callback(callback)
		, log(std::unique_ptr<Log>(callback->GetLog()))
		, game(std::unique_ptr<Game>(callback->GetGame()))
		, map(std::unique_ptr<Map>(callback->GetMap()))
		, lua(std::unique_ptr<Lua>(callback->GetLua()))
		, pathing(std::unique_ptr<Pathing>(callback->GetPathing()))
		, drawer(std::unique_ptr<Drawer>(map->GetDrawer()))
		, skirmishAI(std::unique_ptr<SkirmishAI>(callback->GetSkirmishAI()))
		, airCategory(0)
		, landCategory(0)
		, waterCategory(0)
		, badCategory(0)
		, goodCategory(0)
#ifdef DEBUG_VIS
		, debugDrawer(nullptr)
#endif
{
	ownerTeamId = teamId = skirmishAI->GetTeamId();
	team = std::unique_ptr<Team>(WrappTeam::GetInstance(skirmishAIId, teamId));
	allyTeamId = game->GetMyAllyTeam();
}

CCircuitAI::~CCircuitAI()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	if (isInitialized) {
		Release(0);
	}
}

int CCircuitAI::HandleEvent(int topic, const void* data)
{
	return (this->*eventHandler)(topic, data);
}

void CCircuitAI::NotifyGameEnd()
{
	eventHandler = &CCircuitAI::HandleEndEvent;
}

void CCircuitAI::NotifyResign()
{
	economy = callback->GetEconomy();
	metalRes = callback->GetResourceByName("Metal");
	energyRes = callback->GetResourceByName("Energy");
	eventHandler = &CCircuitAI::HandleResignEvent;
}

//void CCircuitAI::NotifyShutdown()
//{
//	Info* info = skirmishAI->GetInfo();
//	const char* name = info->GetValueByKey("name");
////	OptionValues* options = skirmishAI->GetOptionValues();
////	const char* value = options->GetValueByKey("version");
////	const char* version = (value != nullptr) ? value : info->GetValueByKey("version");
//	delete info;
////	delete options;
//	std::string say("/say "/*"a:"*/);
//	say += std::string(name) + " " + std::string(version) + ": ";
//
//	corrupts.push_front("Error: System files corrupt.");
//	corrupts.push_front("Verifying Checksums...");
//	corrupts.push_front("Loading Kernel...");
//	corrupts.push_front("Terminal connection established.");
//	corrupts.push_back("Saving diagnostics to infolog.txt...");
//	corrupts.push_back("System failure - disconnecting remote users...");
//
//	scheduler->RunTaskEvery(std::make_shared<CGameTask>([this, say]() {
//		if (IsCorrupted()) {
//			game->SendTextMessage((say + corrupts.front()).c_str(), 0);
//			corrupts.pop_front();
//		} else {
//			auto units = std::move(callback->GetTeamUnits());
//			for (Unit* u : units) {
//				if (u != nullptr) {
//					u->SelfDestruct();
//				}
//				delete u;
//			}
//			isResigned = true;  // shutdown
//		}
//	}), FRAMES_PER_SEC * 3);
//
//	eventHandler = &CCircuitAI::HandleShutdownEvent;
//}

void CCircuitAI::Resign(int newTeamId)
{
	ownerTeamId = newTeamId;
	isResigned = true;
}

int CCircuitAI::HandleGameEvent(int topic, const void* data)
{
	int ret = ERROR_UNKNOWN;

	switch (topic) {
		case EVENT_INIT: {
			PRINT_TOPIC("EVENT_INIT", topic);
			SCOPED_TIME(this, "EVENT_INIT");
			struct SInitEvent* evt = (struct SInitEvent*)data;
			try {
				ret = this->Init(evt->skirmishAIId, evt->callback);
			} catch (const CException& e) {
//				corrupts.push_back("!!! " + std::string(e.what()) + " !!!");
				Release(RELEASE_CORRUPTED);
//				scheduler = std::make_shared<CScheduler>();  // NOTE: Don't forget to delete
//				scheduler->Init(scheduler);
//				NotifyShutdown();
				LOG("Exception: %s", e.what());
				NotifyGameEnd();
				ret = 0;
			} catch (...) {
				ret = ERROR_INIT;
			}
			break;
		}
		case EVENT_RELEASE: {
			PRINT_TOPIC("EVENT_RELEASE", topic);
			SCOPED_TIME(this, "EVENT_RELEASE");
			struct SReleaseEvent* evt = (struct SReleaseEvent*)data;
			ret = this->Release(evt->reason);
			break;
		}
		case EVENT_UPDATE: {
//			PRINT_TOPIC("EVENT_UPDATE", topic);
			SCOPED_TIME(this, "EVENT_UPDATE");
			struct SUpdateEvent* evt = (struct SUpdateEvent*)data;
			ret = this->Update(evt->frame);
			break;
		}
		case EVENT_MESSAGE: {
			PRINT_TOPIC("EVENT_MESSAGE", topic);
			SCOPED_TIME(this, "EVENT_MESSAGE");
			struct SMessageEvent* evt = (struct SMessageEvent*)data;
			ret = this->Message(evt->player, evt->message);;
			break;
		}
		case EVENT_UNIT_CREATED: {
			PRINT_TOPIC("EVENT_UNIT_CREATED", topic);
			SCOPED_TIME(this, "EVENT_UNIT_CREATED");
			struct SUnitCreatedEvent* evt = (struct SUnitCreatedEvent*)data;
			CCircuitUnit* builder = GetTeamUnit(evt->builder);
			CCircuitUnit* unit = GetOrRegTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitCreated(unit, builder) : ERROR_UNIT_CREATED;
			break;
		}
		case EVENT_UNIT_FINISHED: {
			PRINT_TOPIC("EVENT_UNIT_FINISHED", topic);
			SCOPED_TIME(this, "EVENT_UNIT_FINISHED");
			struct SUnitFinishedEvent* evt = (struct SUnitFinishedEvent*)data;
			// Lua might call SetUnitHealth within eventHandler.UnitCreated(this, builder);
			// and trigger UnitFinished before eoh->UnitCreated(*this, builder);
			// @see rts/Sim/Units/Unit.cpp CUnit::PostInit
			CCircuitUnit* unit = GetOrRegTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitFinished(unit) : ERROR_UNIT_FINISHED;
			break;
		}
		case EVENT_UNIT_IDLE: {
			PRINT_TOPIC("EVENT_UNIT_IDLE", topic);
			SCOPED_TIME(this, "EVENT_UNIT_IDLE");
			struct SUnitIdleEvent* evt = (struct SUnitIdleEvent*)data;
			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitIdle(unit) : ERROR_UNIT_IDLE;
			break;
		}
		case EVENT_UNIT_MOVE_FAILED: {
			PRINT_TOPIC("EVENT_UNIT_MOVE_FAILED", topic);
			SCOPED_TIME(this, "EVENT_UNIT_MOVE_FAILED");
			struct SUnitMoveFailedEvent* evt = (struct SUnitMoveFailedEvent*)data;
			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitMoveFailed(unit) : ERROR_UNIT_MOVE_FAILED;
			break;
		}
		case EVENT_UNIT_DAMAGED: {
			PRINT_TOPIC("EVENT_UNIT_DAMAGED", topic);
			SCOPED_TIME(this, "EVENT_UNIT_DAMAGED");
			struct SUnitDamagedEvent* evt = (struct SUnitDamagedEvent*)data;
			CEnemyUnit* attacker = GetEnemyUnit(evt->attacker);
			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitDamaged(unit, attacker/*, evt->weaponDefId*/) : ERROR_UNIT_DAMAGED;
			break;
		}
		case EVENT_UNIT_DESTROYED: {
			PRINT_TOPIC("EVENT_UNIT_DESTROYED", topic);
			SCOPED_TIME(this, "EVENT_UNIT_DESTROYED");
			struct SUnitDestroyedEvent* evt = (struct SUnitDestroyedEvent*)data;
			CEnemyUnit* attacker = GetEnemyUnit(evt->attacker);
			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			if (unit != nullptr) {
				ret = this->UnitDestroyed(unit, attacker);
				UnregisterTeamUnit(unit);
			} else {
				ret = ERROR_UNIT_DESTROYED;
			}
			break;
		}
		case EVENT_UNIT_GIVEN: {
			PRINT_TOPIC("EVENT_UNIT_GIVEN", topic);
			SCOPED_TIME(this, "EVENT_UNIT_GIVEN");
			struct SUnitGivenEvent* evt = (struct SUnitGivenEvent*)data;
			ret = this->UnitGiven(evt->unitId, evt->oldTeamId, evt->newTeamId);
			break;
		}
		case EVENT_UNIT_CAPTURED: {
			PRINT_TOPIC("EVENT_UNIT_CAPTURED", topic);
			SCOPED_TIME(this, "EVENT_UNIT_CAPTURED");
			struct SUnitCapturedEvent* evt = (struct SUnitCapturedEvent*)data;
			ret = this->UnitCaptured(evt->unitId, evt->oldTeamId, evt->newTeamId);
			break;
		}
		case EVENT_ENEMY_ENTER_LOS: {
			PRINT_TOPIC("EVENT_ENEMY_ENTER_LOS", topic);
			SCOPED_TIME(this, "EVENT_ENEMY_ENTER_LOS");
			struct SEnemyEnterLOSEvent* evt = (struct SEnemyEnterLOSEvent*)data;
			CEnemyUnit* unit;
			bool isReal;
			std::tie(unit, isReal) = RegisterEnemyUnit(evt->enemy, true);
			if (isReal) {
				ret = (unit != nullptr) ? this->EnemyEnterLOS(unit) : ERROR_ENEMY_ENTER_LOS;
			}
			break;
		}
		case EVENT_ENEMY_LEAVE_LOS: {
			PRINT_TOPIC("EVENT_ENEMY_LEAVE_LOS", topic);
			SCOPED_TIME(this, "EVENT_ENEMY_LEAVE_LOS");
			if (isCheating) {
				ret = 0;
			} else {
				struct SEnemyLeaveLOSEvent* evt = (struct SEnemyLeaveLOSEvent*)data;
				CEnemyUnit* enemy = GetEnemyUnit(evt->enemy);
				ret = (enemy != nullptr) ? this->EnemyLeaveLOS(enemy) : ERROR_ENEMY_LEAVE_LOS;
			}
			break;
		}
		case EVENT_ENEMY_ENTER_RADAR: {
			PRINT_TOPIC("EVENT_ENEMY_ENTER_RADAR", topic);
			SCOPED_TIME(this, "EVENT_ENEMY_ENTER_RADAR");
			struct SEnemyEnterRadarEvent* evt = (struct SEnemyEnterRadarEvent*)data;
			CEnemyUnit* unit;
			bool isReal;
			std::tie(unit, isReal) = RegisterEnemyUnit(evt->enemy, false);
			if (isReal) {
				ret = (unit != nullptr) ? this->EnemyEnterRadar(unit) : ERROR_ENEMY_ENTER_RADAR;
			}
			break;
		}
		case EVENT_ENEMY_LEAVE_RADAR: {
			PRINT_TOPIC("EVENT_ENEMY_LEAVE_RADAR", topic);
			SCOPED_TIME(this, "EVENT_ENEMY_LEAVE_RADAR");
			if (isCheating) {
				ret = 0;
			} else {
				struct SEnemyLeaveRadarEvent* evt = (struct SEnemyLeaveRadarEvent*)data;
				CEnemyUnit* enemy = GetEnemyUnit(evt->enemy);
				ret = (enemy != nullptr) ? this->EnemyLeaveRadar(enemy) : ERROR_ENEMY_LEAVE_RADAR;
			}
			break;
		}
		case EVENT_ENEMY_DAMAGED: {
			PRINT_TOPIC("EVENT_ENEMY_DAMAGED", topic);
			SCOPED_TIME(this, "EVENT_ENEMY_DAMAGED");
			struct SEnemyDamagedEvent* evt = (struct SEnemyDamagedEvent*)data;
			CEnemyUnit* enemy = GetEnemyUnit(evt->enemy);
			ret = (enemy != nullptr) ? this->EnemyDamaged(enemy) : ERROR_ENEMY_DAMAGED;
			break;
		}
		case EVENT_ENEMY_DESTROYED: {
			PRINT_TOPIC("EVENT_ENEMY_DESTROYED", topic);
			SCOPED_TIME(this, "EVENT_ENEMY_DESTROYED");
			struct SEnemyDestroyedEvent* evt = (struct SEnemyDestroyedEvent*)data;
			CEnemyUnit* enemy = GetEnemyUnit(evt->enemy);
			if (enemy != nullptr) {
				ret = this->EnemyDestroyed(enemy);
				UnregisterEnemyUnit(enemy);
			} else {
				ret = ERROR_ENEMY_DESTROYED;
			}
			break;
		}
		case EVENT_WEAPON_FIRED: {
			PRINT_TOPIC("EVENT_WEAPON_FIRED", topic);
			ret = 0;
			break;
		}
		case EVENT_PLAYER_COMMAND: {
			PRINT_TOPIC("EVENT_PLAYER_COMMAND", topic);
			struct SPlayerCommandEvent* evt = (struct SPlayerCommandEvent*)data;
			std::vector<CCircuitUnit*> units;
			units.reserve(evt->unitIds_size);
			for (int i = 0; i < evt->unitIds_size; i++) {
				units.push_back(GetTeamUnit(evt->unitIds[i]));
			}
			ret = this->PlayerCommand(units);
			break;
		}
		case EVENT_SEISMIC_PING: {
			PRINT_TOPIC("EVENT_SEISMIC_PING", topic);
			ret = 0;
			break;
		}
		case EVENT_COMMAND_FINISHED: {
//			PRINT_TOPIC("EVENT_COMMAND_FINISHED", topic);
			// FIXME: commandId always == -1, no use
//			struct SCommandFinishedEvent* evt = (struct SCommandFinishedEvent*)data;
//			CCircuitUnit* unit = GetTeamUnit(evt->unitId);
//			springai::Command* command = WrappCurrentCommand::GetInstance(skirmishAIId, evt->unitId, evt->commandId);
//			this->CommandFinished(unit, evt->commandTopicId, command);
//			delete command;
			ret = 0;
			break;
		}
		case EVENT_LOAD: {
			PRINT_TOPIC("EVENT_LOAD", topic);
			struct SLoadEvent* evt = (struct SLoadEvent*)data;
			std::ifstream loadFileStream;
			loadFileStream.open(evt->file, std::ios::binary);
			ret = loadFileStream.is_open() ? this->Load(loadFileStream) : ERROR_LOAD;
			loadFileStream.close();
			break;
		}
		case EVENT_SAVE: {
			PRINT_TOPIC("EVENT_SAVE", topic);
			struct SSaveEvent* evt = (struct SSaveEvent*)data;
			std::ofstream saveFileStream;
			saveFileStream.open(evt->file, std::ios::binary);
			ret = saveFileStream.is_open() ? this->Save(saveFileStream) : ERROR_SAVE;
			saveFileStream.close();
			break;
		}
		case EVENT_ENEMY_CREATED: {
			PRINT_TOPIC("EVENT_ENEMY_CREATED", topic);
			// @see Cheats::SetEventsEnabled
			// FIXME: Can't query enemy data with globalLOS
			SCOPED_TIME(this, "EVENT_ENEMY_CREATED");
			struct SEnemyCreatedEvent* evt = (struct SEnemyCreatedEvent*)data;
			CEnemyUnit* unit;
			bool isReal;
			std::tie(unit, isReal) = RegisterEnemyUnit(evt->enemy, true);
			if (isReal) {
				ret = (unit != nullptr) ? this->EnemyEnterLOS(unit) : EVENT_ENEMY_CREATED;
			}
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
			SCOPED_TIME(this, "EVENT_LUA_MESSAGE");
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

#ifndef DEBUG_LOG
	ret = 0;
#endif
	return ret;
}

int CCircuitAI::HandleEndEvent(int topic, const void* data)
{
	if (topic == EVENT_RELEASE) {
		PRINT_TOPIC("EVENT_RELEASE::END", topic);
		struct SReleaseEvent* evt = (struct SReleaseEvent*)data;
		return this->Release(evt->reason);
	}
	return 0;
}

int CCircuitAI::HandleResignEvent(int topic, const void* data)
{
	switch (topic) {
		case EVENT_RELEASE: {
			PRINT_TOPIC("EVENT_RELEASE::RESIGN", topic);
			struct SReleaseEvent* evt = (struct SReleaseEvent*)data;
			return this->Release(evt->reason);
		} break;
		case EVENT_UPDATE: {
//			PRINT_TOPIC("EVENT_UPDATE::RESIGN", topic);
			struct SUpdateEvent* evt = (struct SUpdateEvent*)data;
			if (evt->frame % (TEAM_SLOWUPDATE_RATE * INCOME_SAMPLES) == 0) {
				const int mId = metalRes->GetResourceId();
				const int eId = energyRes->GetResourceId();
				float m =  game->GetTeamResourceStorage(ownerTeamId, mId) - HIDDEN_STORAGE - game->GetTeamResourceCurrent(ownerTeamId, mId);
				float e = game->GetTeamResourceStorage(ownerTeamId, eId) - HIDDEN_STORAGE - game->GetTeamResourceCurrent(ownerTeamId, eId);
				m = std::min(economy->GetCurrent(metalRes), std::max(0.f, 0.8f * m));
				e = std::min(economy->GetCurrent(energyRes), std::max(0.f, 0.2f * e));
				economy->SendResource(metalRes, m, ownerTeamId);
				economy->SendResource(energyRes, e, ownerTeamId);
			}
		} break;
		default: break;
	}
	return 0;
}

//int CCircuitAI::HandleShutdownEvent(int topic, const void* data)
//{
//	switch (topic) {
//		case EVENT_RELEASE: {
//			PRINT_TOPIC("EVENT_RELEASE::SHUTDOWN", topic);
//			scheduler = nullptr;  // this->Release() was already called
//			NotifyGameEnd();
//		} break;
//		case EVENT_UPDATE: {
////			PRINT_TOPIC("EVENT_UPDATE::SHUTDOWN", topic);
//			struct SUpdateEvent* evt = (struct SUpdateEvent*)data;
//			scheduler->ProcessTasks(evt->frame);
//			if (isResigned) {
//				scheduler = nullptr;
//				NotifyGameEnd();
//			}
//		} break;
//		default: break;
//	}
//	return 0;
//}

//bool CCircuitAI::IsModValid()
//{
//	const int minEngineVer = 104;
//	const char* engineVersion = sAICallback->Engine_Version_getMajor(skirmishAIId);
//	int ver = atoi(engineVersion);
//	if (ver < minEngineVer) {
//		LOG("Engine must be %i or higher! (Current: %s)", minEngineVer, engineVersion);
//		return false;
//	}
//
//	Mod* mod = callback->GetMod();
//	const char* name = mod->GetHumanName();
//	const char* version = mod->GetVersion();
//	delete mod;
//	if ((name == nullptr) || (version == nullptr)) {
//		LOG("Can't get name or version of the game. Aborting!");  // NOTE: Sign of messed up spring/AI installation
//		return false;
//	}
//
//	if ((strstr(name, "Zero-K") == nullptr)) {
//		LOG("Only Zero-K game is supported! (%s)", name);
//		return false;
//	}
//
//	const int minModVer[] = {1, 6, 4, 0};
//	unsigned i = 0;
//	char* tmp = new char [strlen(version) + 1];
//	strcpy(tmp, version);
//	const char* tok = strtok(tmp, "v.");
//	if (strcmp(tmp, tok) != 0) {  // allow non-standart $VERSION
//		while (tok != nullptr) {
//			int ver = atoi(tok);
//			if (ver < minModVer[i]) {
//				delete[] tmp;
//				LOG("Zero-K must be 1.6.4.0 or higher! (%s)", version);
//				return false;
//			}
//			if ((ver > minModVer[i]) || (++i >= sizeof(minModVer) / sizeof(minModVer[0]))) {
//				break;
//			}
//			tok = strtok(nullptr, ".");
//		}
//	}
//	delete[] tmp;
//
//	return true;
//}

void CCircuitAI::CheatPreload()
{
	auto enemies = std::move(callback->GetEnemyUnits());
	for (Unit* e : enemies) {
		if (e == nullptr) {
			continue;
		}
		CEnemyUnit* enemy = RegisterEnemyUnit(e);
		if (enemy != nullptr) {
			this->EnemyEnterLOS(enemy);
		} else {
			delete e;
		}
	}
}

int CCircuitAI::Init(int skirmishAIId, const struct SSkirmishAICallback* sAICallback)
{
	LOG(version);
	this->skirmishAIId = skirmishAIId;
	// NOTE: Due to chewed API only SSkirmishAICallback have access to Engine
	this->sAICallback = sAICallback;
//	if (!IsModValid()) {
//		return ERROR_INIT;
//	}

#ifdef DEBUG_VIS
	debugDrawer = std::unique_ptr<CDebugDrawer>(new CDebugDrawer(this, sAICallback));
	if (debugDrawer->Init() != 0) {
		return ERROR_INIT;
	}
#endif

	scheduler = std::make_shared<CScheduler>();
	scheduler->Init(scheduler);

	std::string cfgOption = InitOptions();  // Inits GameAttribute
	float decloakRadius;
	InitUnitDefs(decloakRadius);  // Inits TerrainData

	setupManager = std::make_shared<CSetupManager>(this, &gameAttribute->GetSetupData());
	if (!setupManager->OpenConfig(cfgOption)) {
		Release(RELEASE_CONFIG);
		return ERROR_INIT;
	}
	setupManager->ReadConfig();
	if (!setupManager->PickCommander()) {
		Release(RELEASE_COMMANDER);
		return ERROR_INIT;
	}
	allyTeam = setupManager->GetAllyTeam();
	isAllyAware &= allyTeam->GetSize() > 1;

	terrainManager = std::make_shared<CTerrainManager>(this, &gameAttribute->GetTerrainData());
	economyManager = std::make_shared<CEconomyManager>(this);
	threatMap = std::make_shared<CThreatMap>(this, decloakRadius);

	allyTeam->Init(this);
	metalManager = allyTeam->GetMetalManager();
	pathfinder = allyTeam->GetPathfinder();

	terrainManager->Init();

	if (setupManager->HasStartBoxes() && setupManager->CanChooseStartPos()) {
		const CSetupManager::StartPosType spt = metalManager->HasMetalSpots() ?
												CSetupManager::StartPosType::METAL_SPOT :
												CSetupManager::StartPosType::MIDDLE;
		setupManager->PickStartPos(this, spt);
	}

	factoryManager = std::make_shared<CFactoryManager>(this);
	builderManager = std::make_shared<CBuilderManager>(this);
	militaryManager = std::make_shared<CMilitaryManager>(this);

//	InitKnownDefs(setupManager->GetCommChoice());

	// TODO: Remove EconomyManager from module (move abilities to BuilderManager).
	modules.push_back(militaryManager);
	modules.push_back(builderManager);
	modules.push_back(factoryManager);
	modules.push_back(economyManager);  // NOTE: Units use manager, but ain't assigned here

	uEnemyMark = skirmishAIId % FRAMES_PER_SEC;
	kEnemyMark = (skirmishAIId + FRAMES_PER_SEC / 2) % FRAMES_PER_SEC;

	if (isCheating) {
		Cheats* cheats = callback->GetCheats();
		cheats->SetEnabled(true);
		cheats->SetEventsEnabled(true);
		delete cheats;
		scheduler->RunTaskAt(std::make_shared<CGameTask>(&CCircuitAI::CheatPreload, this), skirmishAIId + 1);
	}

	Update(0);  // Init modules: allows to manipulate units on gadget:Initialize
	setupManager->Welcome();

	setupManager->CloseConfig();
	isInitialized = true;

	return 0;  // signaling: OK
}

int CCircuitAI::Release(int reason)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete economy, delete metalRes, delete energyRes;
	economy = nullptr;
	metalRes = energyRes = nullptr;

	if (!isInitialized) {
		return 0;
	}

	if (reason == RELEASE_RESIGN) {
		factoryManager->Release();
		builderManager->Release();
		militaryManager->Release();
	}

	if (reason == 1) {  // @see SReleaseEvent
		gameAttribute->SetGameEnd(true);
	}
	if (terrainManager != nullptr) {
		terrainManager->DidUpdateAreaUsers();
	}

	scheduler->ProcessRelease();
	scheduler = nullptr;

	threatMap = nullptr;
	modules.clear();
	militaryManager = nullptr;
	economyManager = nullptr;
	factoryManager = nullptr;
	builderManager = nullptr;
	terrainManager = nullptr;
	metalManager = nullptr;
	pathfinder = nullptr;
	setupManager = nullptr;

	for (CCircuitUnit* unit : actionUnits) {
		if (teamUnits.find(unit->GetId()) == teamUnits.end()) {
			delete unit;
		}
	}
	actionUnits.clear();
	for (auto& kv : teamUnits) {
		delete kv.second;
	}
	teamUnits.clear();
	garbage.clear();
	for (auto& kv : enemyUnits) {
		delete kv.second;
	}
	enemyUnits.clear();
	if (allyTeam != nullptr) {
		allyTeam->Release();
	}
	for (auto& kv : defsById) {
		delete kv.second;
	}
	defsById.clear();
	defsByName.clear();

	DestroyGameAttribute();

#ifdef DEBUG_VIS
	debugDrawer = nullptr;
#endif

	isInitialized = false;

	return 0;  // signaling: OK
}

int CCircuitAI::Update(int frame)
{
	lastFrame = frame;
	if (isResigned) {
		Release(RELEASE_RESIGN);
		NotifyResign();
		return 0;
	}

	if (!garbage.empty()) {
		CCircuitUnit* unit = *garbage.begin();
		UnitDestroyed(unit, nullptr);
		UnregisterTeamUnit(unit);
		garbage.erase(unit);  // NOTE: UnregisterTeamUnit may erase unit
	}

	if (!enemyUnits.empty()) {
		int mark = frame % FRAMES_PER_SEC;
		if (mark ==  uEnemyMark) {
			UpdateEnemyUnits();
		} else if (mark == kEnemyMark) {
			militaryManager->UpdateEnemyGroups();
		}
	}

	scheduler->ProcessTasks(frame);
	ActionUpdate();

#ifdef DEBUG_VIS
	if (frame % FRAMES_PER_SEC == 0) {
		allyTeam->GetEnergyGrid()->UpdateVis();
		debugDrawer->Refresh();
	}
#endif

	return 0;  // signaling: OK
}

int CCircuitAI::Message(int playerId, const char* message)
{
#ifdef DEBUG_VIS
	const char cmdPos[]    = "~стройсь\0";
	const char cmdSelfD[]  = "~Згинь, нечистая сила!\0";

	const char cmdBlock[]  = "~block\0";
	const char cmdThreat[] = "~threat\0";
	const char cmdArea[]   = "~area\0";
	const char cmdGrid[]   = "~grid\0";
	const char cmdPath[]   = "~path\0";

	const char cmdName[]   = "~name";
	const char cmdEnd[]    = "~end";
//#endif

	if (message[0] != '~') {
		return 0;
	}

	auto selfD = [this]() {
		auto units = std::move(callback->GetTeamUnits());
		for (Unit* u : units) {
			if (u != nullptr) {
				u->SelfDestruct();
			}
			delete u;
		}
	};

	size_t msgLength = strlen(message);

//#ifdef DEBUG_VIS
	if ((msgLength == strlen(cmdPos)) && (strcmp(message, cmdPos) == 0)) {
		setupManager->PickStartPos(this, CSetupManager::StartPosType::RANDOM);
	}
	else if ((msgLength == strlen(cmdSelfD)) && (strcmp(message, cmdSelfD) == 0)) {
		selfD();
	}

	else if ((msgLength == strlen(cmdBlock)) && (strcmp(message, cmdBlock) == 0)) {
		terrainManager->ToggleVis();
	}
	else if ((msgLength == strlen(cmdThreat)) && (strcmp(message, cmdThreat) == 0)) {
		threatMap->ToggleVis();
	}
	else if ((msgLength == strlen(cmdArea)) && (strcmp(message, cmdArea) == 0)) {
		gameAttribute->GetTerrainData().ToggleVis(lastFrame);
	}
	else if ((msgLength == strlen(cmdGrid)) && (strcmp(message, cmdGrid) == 0)) {
		auto selection = std::move(callback->GetSelectedUnits());
		if (!selection.empty()) {
			if (selection[0]->GetAllyTeam() == allyTeamId) {
				allyTeam->GetEnergyGrid()->ToggleVis();
			}
			utils::free_clear(selection);
		} else if (allyTeam->GetEnergyGrid()->IsVis()) {
			allyTeam->GetEnergyGrid()->ToggleVis();
		}
	}
	else if ((msgLength == strlen(cmdPath)) && (strcmp(message, cmdPath) == 0)) {
		pathfinder->ToggleVis(this);
	}

	else if ((strncmp(message, cmdName, 5) == 0)) {
		pathfinder->SetDbgDef(GetCircuitDef(message[6]));
		pathfinder->SetDbgPos(map->GetMousePos());
		const AIFloat3& dbgPos = pathfinder->GetDbgPos();
		LOG("%f, %f, %f, %i", dbgPos.x, dbgPos.y, dbgPos.z, pathfinder->GetDbgDef());
	}
	else if ((strncmp(message, cmdEnd, 4) == 0)) {
		F3Vec path;
		pathfinder->SetDbgType(atoi((const char*)&message[5]));
		AIFloat3 startPos = pathfinder->GetDbgPos();
		AIFloat3 endPos = map->GetMousePos();
		pathfinder->SetMapData(GetThreatMap());
		pathfinder->MakePath(path, startPos, endPos, pathfinder->GetSquareSize());
		LOG("%f, %f, %f, %i", endPos.x, endPos.y, endPos.z, pathfinder->GetDbgType());
	}
#endif

	return 0;  // signaling: OK
}

int CCircuitAI::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	for (auto& module : modules) {
		module->UnitCreated(unit, builder);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitFinished(CCircuitUnit* unit)
{
	if (unit->GetUnit()->IsBeingBuilt()) {
		return 0;  // created by gadget
	}
	TRY_UNIT(this, unit,
		unit->GetUnit()->ExecuteCustomCommand(CMD_DONT_FIRE_AT_RADAR, {0.0f});
		if (unit->GetCircuitDef()->GetUnitDef()->IsAbleToCloak()) {
			unit->GetUnit()->ExecuteCustomCommand(CMD_WANT_CLOAK, {1.0f});  // personal
			unit->GetUnit()->ExecuteCustomCommand(CMD_CLOAK_SHIELD, {1.0f});  // area
			unit->GetUnit()->Cloak(true);
		}
	)
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

int CCircuitAI::UnitMoveFailed(CCircuitUnit* unit)
{
	if (unit->IsMoveFailed(lastFrame)) {
		TRY_UNIT(this, unit,
			unit->GetUnit()->Stop();
			unit->GetUnit()->SetMoveState(2);
		)
		UnitDestroyed(unit, nullptr);
		UnregisterTeamUnit(unit);
	} else if (unit->GetTask() != nullptr) {
		unit->GetTask()->OnUnitMoveFailed(unit);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker/*, int weaponId*/)
{
	// FIXME: Doesn't work well with multi-shots (duck)
//	if (lastFrame <= unit->GetDamagedFrame() + FRAMES_PER_SEC / 5) {
//		return 0;
//	}
//	unit->SetDamagedFrame(lastFrame);

	for (auto& module : modules) {
		module->UnitDamaged(unit, attacker);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	for (auto& module : modules) {
		module->UnitDestroyed(unit, attacker);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitGiven(ICoreUnit::Id unitId, int oldTeamId, int newTeamId)
{
	CEnemyUnit* enemy = GetEnemyUnit(unitId);
	if (enemy != nullptr) {
		EnemyDestroyed(enemy);
		UnregisterEnemyUnit(enemy);
	}

	// it might not have been given to us! Could have been given to another team
	if (teamId != newTeamId) {
		return 0;  // signaling: OK
	}

	CCircuitUnit* unit = GetOrRegTeamUnit(unitId);
	if (unit == nullptr) {
		return ERROR_UNIT_GIVEN;
	}

	TRY_UNIT(this, unit,
		unit->GetUnit()->Stop();
		unit->GetUnit()->ExecuteCustomCommand(CMD_DONT_FIRE_AT_RADAR, {0.0f});
		if (unit->GetCircuitDef()->GetUnitDef()->IsAbleToCloak()) {
			unit->GetUnit()->ExecuteCustomCommand(CMD_WANT_CLOAK, {1.0f});  // personal
			unit->GetUnit()->ExecuteCustomCommand(CMD_CLOAK_SHIELD, {1.0f});  // area
			unit->GetUnit()->Cloak(true);
		}
	)
	for (auto& module : modules) {
		module->UnitGiven(unit, oldTeamId, newTeamId);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitCaptured(ICoreUnit::Id unitId, int oldTeamId, int newTeamId)
{
	// it might not have been captured from us! Could have been captured from another team
	if (teamId != oldTeamId) {
		return 0;  // signaling: OK
	}

	CCircuitUnit* unit = GetTeamUnit(unitId);
	if (unit == nullptr) {
		return ERROR_UNIT_CAPTURED;
	}

	for (auto& module : modules) {
		module->UnitCaptured(unit, oldTeamId, newTeamId);
	}

	UnregisterTeamUnit(unit);

	return 0;  // signaling: OK
}

int CCircuitAI::EnemyEnterLOS(CEnemyUnit* enemy)
{
	bool isKnownBefore = enemy->IsKnown() && (enemy->IsInRadar() || !enemy->GetCircuitDef()->IsMobile());

	if (threatMap->EnemyEnterLOS(enemy)) {
		militaryManager->AddEnemyCost(enemy);
	}

	if (isKnownBefore) {
		return 0;  // signaling: OK
	}
	// Force unit's reaction
	auto friendlies = std::move(callback->GetFriendlyUnitsIn(enemy->GetPos(), 500.0f));
	if (friendlies.empty()) {
		return 0;  // signaling: OK
	}
	for (Unit* f : friendlies) {
		if (f == nullptr) {
			continue;
		}
		CCircuitUnit* unit = GetTeamUnit(f->GetUnitId());
		if ((unit != nullptr) && (unit->GetTask() != nullptr)) {
			unit->ForceExecute();
		}
		delete f;
	}

	return 0;  // signaling: OK
}

int CCircuitAI::EnemyLeaveLOS(CEnemyUnit* enemy)
{
	threatMap->EnemyLeaveLOS(enemy);

	return 0;  // signaling: OK
}

int CCircuitAI::EnemyEnterRadar(CEnemyUnit* enemy)
{
	threatMap->EnemyEnterRadar(enemy);
	enemy->SetLastSeen(-1);

	return 0;  // signaling: OK
}

int CCircuitAI::EnemyLeaveRadar(CEnemyUnit* enemy)
{
	threatMap->EnemyLeaveRadar(enemy);
	enemy->SetLastSeen(lastFrame);

	return 0;  // signaling: OK
}

int CCircuitAI::EnemyDamaged(CEnemyUnit* enemy)
{
	threatMap->EnemyDamaged(enemy);

	return 0;  // signaling: OK
}

int CCircuitAI::EnemyDestroyed(CEnemyUnit* enemy)
{
	if (threatMap->EnemyDestroyed(enemy)) {
		militaryManager->DelEnemyCost(enemy);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::PlayerCommand(std::vector<CCircuitUnit*>& units)
{
	for (CCircuitUnit* unit : units) {
		if ((unit != nullptr) && (unit->GetTask() != nullptr) &&
			(unit->GetTask()->GetType() != IUnitTask::Type::NIL) &&  // ignore orders to nanoframes
			(unit->GetTask()->GetType() != IUnitTask::Type::PLAYER))
		{
			ITaskManager* mgr = unit->GetTask()->GetManager();
			mgr->AssignTask(unit, new CPlayerTask(mgr));
		}
	}

	return 0;  // signaling: OK
}

//int CCircuitAI::CommandFinished(CCircuitUnit* unit, int commandTopicId, springai::Command* cmd)
//{
//	for (auto& module : modules) {
//		module->CommandFinished(unit, commandTopicId);
//	}
//
//	return 0;  // signaling: OK
//}

int CCircuitAI::Load(std::istream& is)
{
	auto units = std::move(callback->GetTeamUnits());
	for (Unit* u : units) {
		if (u == nullptr) {
			continue;
		}
		ICoreUnit::Id unitId = u->GetUnitId();
		CCircuitUnit* unit = GetTeamUnit(unitId);
		if (unit != nullptr) {
			continue;
		}
		unit = RegisterTeamUnit(unitId);
		if (unit == nullptr) {
			continue;
		}
		u->IsBeingBuilt() ? UnitCreated(unit, nullptr) : UnitFinished(unit);
	}
	utils::free_clear(units);

	for (auto& module : modules) {
		is >> *module;
	}

	return 0;  // signaling: OK
}

int CCircuitAI::Save(std::ostream& os)
{
	for (auto& module : modules) {
		os << *module;
	}

	return 0;  // signaling: OK
}

int CCircuitAI::LuaMessage(const char* inData)
{
	if (strncmp(inData, "DISABLE_CONTROL:", 16) == 0) {
		DisableControl(inData + 16);
	} else
	if (strncmp(inData, "ENABLE_CONTROL:", 15) == 0) {
		EnableControl(inData + 15);
	}
	return 0;  // signaling: OK
}

CCircuitUnit* CCircuitAI::GetOrRegTeamUnit(ICoreUnit::Id unitId)
{
	CCircuitUnit* unit = GetTeamUnit(unitId);
	if (unit != nullptr) {
		return unit;
	}

	return RegisterTeamUnit(unitId);
}

CCircuitUnit* CCircuitAI::RegisterTeamUnit(ICoreUnit::Id unitId)
{
	Unit* u = WrappUnit::GetInstance(skirmishAIId, unitId);
	if (u == nullptr) {
		return nullptr;
	}
	UnitDef* unitDef = u->GetDef();
	CCircuitDef* cdef = GetCircuitDef(unitDef->GetUnitDefId());
	CCircuitUnit* unit = new CCircuitUnit(unitId, u, cdef);
	delete unitDef;

	STerrainMapArea* area;
	bool isValid;
	std::tie(area, isValid) = terrainManager->GetCurrentMapArea(cdef, unit->GetPos(lastFrame));
	unit->SetArea(area);

	teamUnits[unitId] = unit;
	cdef->Inc();

	if (!isValid) {
		Garbage(unit, "useless");
	}
	return unit;
}

void CCircuitAI::UnregisterTeamUnit(CCircuitUnit* unit)
{
	teamUnits.erase(unit->GetId());
	defsById[unit->GetCircuitDef()->GetId()]->Dec();

	(unit->GetTask() == nullptr) ? DeleteTeamUnit(unit) : unit->Dead();
}

void CCircuitAI::DeleteTeamUnit(CCircuitUnit* unit)
{
	garbage.erase(unit);
	delete unit;
}

void CCircuitAI::Garbage(CCircuitUnit* unit, const char* reason)
{
	// NOTE: Happens because engine can send EVENT_UNIT_FINISHED after EVENT_UNIT_DESTROYED.
	//       Engine should not send events with isDead units.
	garbage.insert(unit);
#ifdef DEBUG_LOG
	LOG("AI: %i | Garbage unit: %i | reason: %s", skirmishAIId, unit->GetId(), reason);
#endif
}

CCircuitUnit* CCircuitAI::GetTeamUnit(ICoreUnit::Id unitId) const
{
	auto it = teamUnits.find(unitId);
	return (it != teamUnits.end()) ? it->second : nullptr;
}

CAllyUnit* CCircuitAI::GetFriendlyUnit(Unit* u) const
{
	if (u == nullptr) {
		return nullptr;
	}

	if (u->GetTeam() == teamId) {
		return GetTeamUnit(u->GetUnitId());
	} else if (u->GetAllyTeam() == allyTeamId) {
		return allyTeam->GetFriendlyUnit(u->GetUnitId());
	}

	return nullptr;
}

std::pair<CEnemyUnit*, bool> CCircuitAI::RegisterEnemyUnit(ICoreUnit::Id unitId, bool isInLOS)
{
	CEnemyUnit* unit = GetEnemyUnit(unitId);
	if (unit != nullptr) {
		if (isInLOS/* && (unit->GetCircuitDef() == nullptr)*/) {
			UnitDef* unitDef = unit->GetUnit()->GetDef();
			if (unitDef == nullptr) {  // doesn't work with globalLOS
				return std::make_pair(nullptr, false);
			}
			CCircuitDef::Id unitDefId = unitDef->GetUnitDefId();
			delete unitDef;
			if ((unit->GetCircuitDef() == nullptr) || unit->GetCircuitDef()->GetId() != unitDefId) {
				unit->SetCircuitDef(defsById[unitDefId]);
				unit->SetCost(unit->GetUnit()->GetRulesParamFloat("comm_cost", unit->GetCost()));
			}
		}
		return std::make_pair(unit, true);
	}

	Unit* u = WrappUnit::GetInstance(skirmishAIId, unitId);
	if (u == nullptr) {
		return std::make_pair(nullptr, true);
	}
	if (/*u->IsNeutral() || */u->GetRulesParamFloat("ignoredByAI", 0.f) > 0.f) {
		delete u;
		return std::make_pair(nullptr, false);
	}
	CCircuitDef* cdef = nullptr;
	if (isInLOS) {
		UnitDef* unitDef = u->GetDef();
		if (unitDef == nullptr) {  // doesn't work with globalLOS
			delete u;
			return std::make_pair(nullptr, false);
		}
		cdef = defsById[unitDef->GetUnitDefId()];
		delete unitDef;
	}
	unit = new CEnemyUnit(unitId, u, cdef);

	enemyUnits[unit->GetId()] = unit;

	return std::make_pair(unit, true);
}

CEnemyUnit* CCircuitAI::RegisterEnemyUnit(Unit* e)
{
	if (/*e->IsNeutral() || */e->GetRulesParamFloat("ignoredByAI", 0.f) > 0.f) {
		return nullptr;
	}

	const ICoreUnit::Id unitId = e->GetUnitId();
	CEnemyUnit* unit = GetEnemyUnit(unitId);
	UnitDef* unitDef = e->GetDef();
	CCircuitDef::Id unitDefId = unitDef->GetUnitDefId();
	delete unitDef;

	if (unit != nullptr) {
		if ((unit->GetCircuitDef() == nullptr) || unit->GetCircuitDef()->GetId() != unitDefId) {
			unit->SetCircuitDef(defsById[unitDefId]);
			unit->SetCost(unit->GetUnit()->GetRulesParamFloat("comm_cost", unit->GetCost()));
		}
		return nullptr;
	}

	CCircuitDef* cdef = defsById[unitDefId];
	unit = new CEnemyUnit(unitId, e, cdef);
	enemyUnits[unit->GetId()] = unit;
	return unit;
}

void CCircuitAI::UnregisterEnemyUnit(CEnemyUnit* unit)
{
	enemyUnits.erase(unit->GetId());
	delete unit;
}

void CCircuitAI::UpdateEnemyUnits()
{
	auto it = enemyUnits.begin();
	while (it != enemyUnits.end()) {
		CEnemyUnit* enemy = it->second;

		int frame = enemy->GetLastSeen();
		if ((frame != -1) && (lastFrame - frame >= FRAMES_PER_SEC * 600)) {
			EnemyDestroyed(enemy);
			it = enemyUnits.erase(it);  // UnregisterEnemyUnit(enemy)
			delete enemy;
			continue;
		}

		if (enemy->IsInRadarOrLOS()) {
			const AIFloat3& pos = enemy->GetUnit()->GetPos();
			if (CTerrainData::IsNotInBounds(pos)) {  // FIXME: Unit id validation. No EnemyDestroyed sometimes apparently
				EnemyDestroyed(enemy);
				it = enemyUnits.erase(it);  // UnregisterEnemyUnit(enemy)
				delete enemy;
				continue;
			}
			enemy->SetNewPos(pos);
		}

		++it;
	}

	threatMap->Update();
}

CEnemyUnit* CCircuitAI::GetEnemyUnit(ICoreUnit::Id unitId) const
{
	auto it = enemyUnits.find(unitId);
	return (it != enemyUnits.end()) ? it->second : nullptr;
}

void CCircuitAI::DisableControl(const std::string data)
{
	std::smatch section;
	std::string::const_iterator start = data.begin();
	std::string::const_iterator end = data.end();
	std::regex patternUnit("\\w+");
	while (std::regex_search(start, end, section, patternUnit)) {
		CCircuitUnit* unit = GetTeamUnit(utils::string_to_int(section[0]));
		if ((unit != nullptr) && (unit->GetTask() != nullptr)) {
			ITaskManager* mgr = unit->GetTask()->GetManager();
			mgr->AssignTask(unit, new CPlayerTask(mgr));
		}
		start = section[0].second;
	}
}

void CCircuitAI::EnableControl(const std::string data)
{
	std::smatch section;
	std::string::const_iterator start = data.begin();
	std::string::const_iterator end = data.end();
	std::regex patternUnit("\\w+");
	while (std::regex_search(start, end, section, patternUnit)) {
		CCircuitUnit* unit = GetTeamUnit(utils::string_to_int(section[0]));
		if ((unit != nullptr) && (unit->GetTask() != nullptr)) {
			unit->GetTask()->RemoveAssignee(unit);
		}
		start = section[0].second;
	}
}

void CCircuitAI::ActionUpdate()
{
	if (actionIterator >= actionUnits.size()) {
		actionIterator = 0;
	}

	// stagger the Update's
	unsigned int n = (actionUnits.size() / ACTION_UPDATE_RATE) + 1;

	while ((actionIterator < actionUnits.size()) && (n != 0)) {
		CCircuitUnit* unit = actionUnits[actionIterator];
		if (unit->IsDead()) {
			actionUnits[actionIterator] = actionUnits.back();
			actionUnits.pop_back();
			DeleteTeamUnit(unit);
		} else {
			if (unit->GetTask()->GetType() != IUnitTask::Type::PLAYER) {
				unit->Update(this);
			}
			++actionIterator;
			n--;
		}
	}
}

std::string CCircuitAI::InitOptions()
{
	OptionValues* options = skirmishAI->GetOptionValues();
	const char* value;

	value = options->GetValueByKey("cheating");
	if (value != nullptr) {
		isCheating = StringToBool(value);
	}

	value = options->GetValueByKey("ally_aware");
	if (value != nullptr) {
		isAllyAware = StringToBool(value);
	}

	value = options->GetValueByKey("comm_merge");
	if (value != nullptr) {
		isCommMerge = StringToBool(value);
	}

	value = options->GetValueByKey("config_file");
	std::string cfgOption = ((value != nullptr) && strlen(value) > 0) ? value : "";

	value = options->GetValueByKey("random_seed");
	unsigned int seed = (value != nullptr) ? StringToInt(value) : time(nullptr);
	CreateGameAttribute(seed);

	delete options;
	return cfgOption;
}

CCircuitDef* CCircuitAI::GetCircuitDef(const char* name)
{
	auto it = defsByName.find(name);
	// NOTE: For the sake of AI's health it should not return nullptr
	return (it != defsByName.end()) ? it->second : nullptr;
}

CCircuitDef* CCircuitAI::GetCircuitDef(CCircuitDef::Id unitDefId)
{
	auto it = defsById.find(unitDefId);
	return (it != defsById.end()) ? it->second : nullptr;
}

void CCircuitAI::InitUnitDefs(float& outDcr)
{
	airCategory   = game->GetCategoriesFlag("FIXEDWING GUNSHIP");
	landCategory  = game->GetCategoriesFlag("LAND SINK TURRET SHIP SWIM FLOAT HOVER");
	waterCategory = game->GetCategoriesFlag("SUB");
	badCategory   = game->GetCategoriesFlag("TERRAFORM STUPIDTARGET MINE");
	goodCategory  = game->GetCategoriesFlag("TURRET FLOAT");

	if (!gameAttribute->GetTerrainData().IsInitialized()) {
		gameAttribute->GetTerrainData().Init(this);
	}
	Resource* res = callback->GetResourceByName("Metal");
	outDcr = 0.f;
	const std::vector<UnitDef*>& unitDefs = callback->GetUnitDefs();
	for (UnitDef* ud : unitDefs) {
		auto options = std::move(ud->GetBuildOptions());
		std::unordered_set<CCircuitDef::Id> opts;
		for (UnitDef* buildDef : options) {
			opts.insert(buildDef->GetUnitDefId());
			delete buildDef;
		}
		CCircuitDef* cdef = new CCircuitDef(this, ud, opts, res);

		defsByName[ud->GetName()] = cdef;
		defsById[cdef->GetId()] = cdef;

		const float dcr = ud->GetDecloakDistance();
		if (outDcr < dcr) {
			outDcr = dcr;
		}
	}
	delete res;

	for (auto& kv : GetCircuitDefs()) {
		kv.second->Init(this);
	}
}

//void CCircuitAI::InitKnownDefs(const CCircuitDef* commDef)
//{
//	std::set<CCircuitDef::Id> visited;
//	std::queue<CCircuitDef::Id> queue;
//
//	queue.push(commDef->GetId());
//
//	while (!queue.empty()) {
//		CCircuitDef* cdef = GetCircuitDef(queue.front());
//		queue.pop();
//
//		visited.insert(cdef->GetId());
//		for (CCircuitDef::Id child : cdef->GetBuildOptions()) {
//			if (visited.find(child) == visited.end()) {
//				queue.push(child);
//			}
//		}
//	}
//
//	knownDefs.reserve(visited.size());
//	for (CCircuitDef::Id id : visited) {
//		knownDefs.push_back(GetCircuitDef(id));
//	}
//}

//// debug
//void CCircuitAI::DrawClusters()
//{
//	gameAttribute->GetMetalData().DrawConvexHulls(GetDrawer());
//	gameAttribute->GetMetalManager().DrawCentroids(GetDrawer());
//}

void CCircuitAI::CreateGameAttribute(unsigned int seed)
{
	if (gameAttribute == nullptr) {
		gameAttribute = std::unique_ptr<CGameAttribute>(new CGameAttribute(seed));
	}
	gaCounter++;
	gameAttribute->RegisterAI(this);
}

void CCircuitAI::DestroyGameAttribute()
{
	gameAttribute->UnregisterAI(this);
	if (gaCounter <= 1) {
		if (gameAttribute != nullptr) {
			gameAttribute = nullptr;  // deletes singleton here;
		}
		gaCounter = 0;
	} else {
		gaCounter--;
	}
}

} // namespace circuit
