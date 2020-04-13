/*
 * Circuit.cpp
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#include "CircuitAI.h"
#include "script/ScriptManager.h"
#include "setup/SetupManager.h"
#include "map/MapManager.h"
#include "map/ThreatMap.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "module/EconomyManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/PathFinder.h"
#include "task/PlayerTask.h"
#include "unit/CircuitUnit.h"
#include "unit/enemy/EnemyUnit.h"
#include "util/GameAttribute.h"
#include "util/Scheduler.h"
#include "util/Utils.h"
#ifdef DEBUG_VIS
#include "map/InfluenceMap.h"
#include "map/ThreatMap.h"
#include "resource/EnergyGrid.h"
#endif  // DEBUG_VIS

#include "spring/SpringCallback.h"
#include "spring/SpringEngine.h"
#include "spring/SpringMap.h"

#include "AISEvents.h"
#include "AISCommands.h"
#include "Log.h"
#include "Game.h"
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

#define ACTION_UPDATE_RATE	64
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

CCircuitAI::CCircuitAI(OOAICallback* clb)
		: eventHandler(&CCircuitAI::HandleGameEvent)
		, economy(nullptr)
		, metalRes(nullptr)
		, energyRes(nullptr)
		, allyTeam(nullptr)
		, actionIterator(0)
		, isCheating(false)
		, isAllyAware(true)
		, isCommMerge(true)
		, isInitialized(false)
		, isLoadSave(false)
		, isResigned(false)
		// NOTE: assert(lastFrame != -1): CCircuitUnit initialized with -1
		//       and lastFrame check will misbehave until first update event.
		, lastFrame(-2)
		, skirmishAIId(clb != nullptr ? clb->GetSkirmishAIId() : -1)
		, callback(std::unique_ptr<COOAICallback>(new COOAICallback(clb)))
		, engine(nullptr)
		, cheats(std::unique_ptr<Cheats>(clb->GetCheats()))
		, log(std::unique_ptr<Log>(clb->GetLog()))
		, game(std::unique_ptr<Game>(clb->GetGame()))
		, map(nullptr)
		, lua(std::unique_ptr<Lua>(clb->GetLua()))
		, pathing(std::unique_ptr<Pathing>(clb->GetPathing()))
		, drawer(nullptr)
		, skirmishAI(std::unique_ptr<SkirmishAI>(clb->GetSkirmishAI()))
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
				Release(RELEASE_CORRUPTED);
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
			CEnemyInfo* attacker = GetEnemyInfo(evt->attacker);
			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitDamaged(unit, attacker/*, evt->weaponDefId*/) : ERROR_UNIT_DAMAGED;
			break;
		}
		case EVENT_UNIT_DESTROYED: {
			PRINT_TOPIC("EVENT_UNIT_DESTROYED", topic);
			SCOPED_TIME(this, "EVENT_UNIT_DESTROYED");
			struct SUnitDestroyedEvent* evt = (struct SUnitDestroyedEvent*)data;
			CEnemyInfo* attacker = GetEnemyInfo(evt->attacker);
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
			CEnemyInfo* unit;
			bool isReal;
			std::tie(unit, isReal) = RegisterEnemyInfo(evt->enemy, true);
			ret = isReal
					? (unit != nullptr) ? this->EnemyEnterLOS(unit) : ERROR_ENEMY_ENTER_LOS
					: 0;
			break;
		}
		case EVENT_ENEMY_LEAVE_LOS: {
			PRINT_TOPIC("EVENT_ENEMY_LEAVE_LOS", topic);
			SCOPED_TIME(this, "EVENT_ENEMY_LEAVE_LOS");
			if (isCheating) {
				ret = 0;
			} else {
				struct SEnemyLeaveLOSEvent* evt = (struct SEnemyLeaveLOSEvent*)data;
				CEnemyInfo* enemy = GetEnemyInfo(evt->enemy);
				ret = (enemy != nullptr) ? this->EnemyLeaveLOS(enemy) : ERROR_ENEMY_LEAVE_LOS;
			}
			break;
		}
		case EVENT_ENEMY_ENTER_RADAR: {
			PRINT_TOPIC("EVENT_ENEMY_ENTER_RADAR", topic);
			SCOPED_TIME(this, "EVENT_ENEMY_ENTER_RADAR");
			struct SEnemyEnterRadarEvent* evt = (struct SEnemyEnterRadarEvent*)data;
			CEnemyInfo* unit;
			bool isReal;
			std::tie(unit, isReal) = RegisterEnemyInfo(evt->enemy, false);
			ret = isReal
					? (unit != nullptr) ? this->EnemyEnterRadar(unit) : ERROR_ENEMY_ENTER_RADAR
					: 0;
			break;
		}
		case EVENT_ENEMY_LEAVE_RADAR: {
			PRINT_TOPIC("EVENT_ENEMY_LEAVE_RADAR", topic);
			SCOPED_TIME(this, "EVENT_ENEMY_LEAVE_RADAR");
			if (isCheating) {
				ret = 0;
			} else {
				struct SEnemyLeaveRadarEvent* evt = (struct SEnemyLeaveRadarEvent*)data;
				CEnemyInfo* enemy = GetEnemyInfo(evt->enemy);
				ret = (enemy != nullptr) ? this->EnemyLeaveRadar(enemy) : ERROR_ENEMY_LEAVE_RADAR;
			}
			break;
		}
		case EVENT_ENEMY_DAMAGED: {
			PRINT_TOPIC("EVENT_ENEMY_DAMAGED", topic);
			SCOPED_TIME(this, "EVENT_ENEMY_DAMAGED");
			struct SEnemyDamagedEvent* evt = (struct SEnemyDamagedEvent*)data;
			CEnemyInfo* enemy = GetEnemyInfo(evt->enemy);
			ret = (enemy != nullptr) ? this->EnemyDamaged(enemy) : ERROR_ENEMY_DAMAGED;
			break;
		}
		case EVENT_ENEMY_DESTROYED: {
			PRINT_TOPIC("EVENT_ENEMY_DESTROYED", topic);
			SCOPED_TIME(this, "EVENT_ENEMY_DESTROYED");
			struct SEnemyDestroyedEvent* evt = (struct SEnemyDestroyedEvent*)data;
			CEnemyInfo* enemy = GetEnemyInfo(evt->enemy);
			if (enemy != nullptr) {
				ret = this->EnemyDestroyed(enemy);
				UnregisterEnemyInfo(enemy);
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
			CEnemyInfo* unit;
			bool isReal;
			std::tie(unit, isReal) = RegisterEnemyInfo(evt->enemy, true);
			ret = isReal
					? (unit != nullptr) ? this->EnemyEnterLOS(unit) : EVENT_ENEMY_CREATED
					: 0;
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

//bool CCircuitAI::IsModValid()
//{
//	const int minEngineVer = 104;
//	const char* engineVersion = engine->GetVersionMajor();
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
	auto enemies = callback->GetEnemyUnits();
	for (Unit* e : enemies) {
		if (e == nullptr) {
			continue;
		}
		CEnemyInfo* enemy = RegisterEnemyInfo(e);
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
	callback->Init(sAICallback);
	engine = std::unique_ptr<CEngine>(new CEngine(sAICallback, skirmishAIId));
	map = std::unique_ptr<CMap>(new CMap(sAICallback, callback->GetMap()));
	drawer = std::unique_ptr<Drawer>(map->GetDrawer());
//	if (!IsModValid()) {
//		return ERROR_INIT;
//	}

#ifdef DEBUG_VIS
	debugDrawer = std::unique_ptr<CDebugDrawer>(new CDebugDrawer(this, sAICallback));
	if (debugDrawer->Init() != 0) {
		return ERROR_INIT;
	}
#endif

	CreateGameAttribute();
	scheduler = std::make_shared<CScheduler>();
	scheduler->Init(scheduler);
	scriptManager = std::make_shared<CScriptManager>(this);

	std::string cfgOption = InitOptions();  // Inits GameAttribute
	InitWeaponDefs();
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

	allyTeam->Init(this, decloakRadius);
	mapManager = allyTeam->GetMapManager();
	enemyManager = allyTeam->GetEnemyManager();
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

	scriptManager->RegisterMgr();
	for (auto& module : modules) {
		module->InitScript();
	}

	if (isCheating) {
		cheats->SetEnabled(true);
		cheats->SetEventsEnabled(true);
		scheduler->RunTaskAt(std::make_shared<CGameTask>(&CCircuitAI::CheatPreload, this), skirmishAIId + 1);
	}

	scheduler->ProcessInit();  // Init modules: allows to manipulate units on gadget:Initialize
	setupManager->Welcome();

	setupManager->CloseConfig();
	isInitialized = true;

	return 0;  // signaling: OK
}

int CCircuitAI::Release(int reason)
{
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
		terrainManager->OnAreaUsersUpdated();
	}

	scheduler->ProcessRelease();
	scheduler = nullptr;

	modules.clear();
	scriptManager = nullptr;
	militaryManager = nullptr;
	economyManager = nullptr;
	factoryManager = nullptr;
	builderManager = nullptr;
	terrainManager = nullptr;
	metalManager = nullptr;
	pathfinder = nullptr;
	setupManager = nullptr;
	enemyManager = nullptr;
	mapManager = nullptr;

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
	for (auto& kv : enemyInfos) {
		delete kv.second;
	}
	enemyInfos.clear();
	if (allyTeam != nullptr) {
		allyTeam->Release();
	}
	for (auto& kv : defsById) {
		delete kv.second;
	}
	defsById.clear();
	defsByName.clear();
	utils::free_clear(weaponDefs);

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

	if (!allyTeam->GetEnemyGarbage().empty()) {
		for (const ICoreUnit::Id eId : allyTeam->GetEnemyGarbage()) {
			CEnemyInfo* enemy = GetEnemyInfo(eId);
			if (enemy != nullptr) {  // EnemyDestroyed right after UpdateEnemyDatas but before this Update
				EnemyDestroyed(enemy);
				UnregisterEnemyInfo(enemy);
			}
		}
	}

	if (!enemyInfos.empty()) {
		allyTeam->Update(this);
	}

	scheduler->ProcessTasks(frame);
	UpdateActions();

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
	const char cmdPos[]     = "~стройсь\0";
	const char cmdSelfD[]   = "~Згинь, нечистая сила!\0";

	const char cmdBlock[]   = "~block";
	const char cmdArea[]    = "~area";
	const char cmdPath[]    = "~path";
	const char cmdKnn[]     = "~knn";
	const char cmdLog[]     = "~log";

	const char cmdThreat[]  = "~threat";
	const char cmdWTDraw[]  = "~wtdraw";
	const char cmdWTDiv[]   = "~wtdiv";
	const char cmdWTPrint[] = "~wtprint";

	const char cmdInfl[]    = "~infl";
	const char cmdWIDraw[]  = "~widraw";
	const char cmdWIDiv[]   = "~widiv";
	const char cmdWIPrint[] = "~wiprint";

	const char cmdGrid[]    = "~grid";
	const char cmdNode[]    = "~node";
	const char cmdLink[]    = "~link";

	const char cmdName[]    = "~name";
	const char cmdEnd[]     = "~end";

	if (message[0] != '~') {
		return 0;
	}

	auto selfD = [this]() {
		auto units = callback->GetTeamUnits();
		for (Unit* u : units) {
			if (u != nullptr) {
				u->SelfDestruct();
			}
			delete u;
		}
	};

	size_t msgLength = strlen(message);

	if ((msgLength == strlen(cmdPos)) && (strcmp(message, cmdPos) == 0)) {
		setupManager->PickStartPos(this, CSetupManager::StartPosType::RANDOM);
	}
	else if ((msgLength == strlen(cmdSelfD)) && (strcmp(message, cmdSelfD) == 0)) {
		selfD();
	}

	else if (strncmp(message, cmdBlock, 6) == 0) {
		terrainManager->ToggleVis();
	}
	else if (strncmp(message, cmdArea, 5) == 0) {
		gameAttribute->GetTerrainData().ToggleVis(lastFrame);
	}
	else if (strncmp(message, cmdPath, 5) == 0) {
		pathfinder->ToggleVis(lastFrame);
	}
	else if (strncmp(message, cmdKnn, 4) == 0) {
		const AIFloat3 dbgPos = map->GetMousePos();
		int index = metalManager->FindNearestCluster(dbgPos);
		drawer->AddPoint(metalManager->GetClusters()[index].position, "knn");
	}
	else if (strncmp(message, cmdLog, 4) == 0) {
		auto selection = callback->GetSelectedUnits();
		for (Unit* u : selection) {
			CCircuitUnit* unit = GetTeamUnit(u->GetUnitId());
			if (unit != nullptr) {
				unit->Log();
			}
		}
		utils::free_clear(selection);
	}

	else if (strncmp(message, cmdThreat, 7) == 0) {
		mapManager->GetThreatMap()->ToggleSDLVis();
	}
	else if (strncmp(message, cmdWTDraw, 7) == 0) {
		if (teamId == atoi((const char*)&message[8])) {
			mapManager->GetThreatMap()->ToggleWidgetDraw();
		}
	}
	else if (strncmp(message, cmdWTDiv, 6) == 0) {
		mapManager->GetThreatMap()->SetMaxThreat(atof((const char*)&message[7]));
	}
	else if (strncmp(message, cmdWTPrint, 8) == 0) {
		if (teamId == atoi((const char*)&message[9])) {
			mapManager->GetThreatMap()->ToggleWidgetPrint();
		}
	}

	else if (strncmp(message, cmdInfl, 5) == 0) {
		mapManager->GetInflMap()->ToggleSDLVis();
	}
	else if (strncmp(message, cmdWIDraw, 7) == 0) {
		if (teamId == atoi((const char*)&message[8])) {
			mapManager->GetInflMap()->ToggleWidgetDraw();
		}
	}
	else if (strncmp(message, cmdWIDiv, 6) == 0) {
		mapManager->GetInflMap()->SetMaxThreat(atof((const char*)&message[7]));
	}
	else if (strncmp(message, cmdWIPrint, 8) == 0) {
		if (teamId == atoi((const char*)&message[9])) {
			mapManager->GetInflMap()->ToggleWidgetPrint();
		}
	}

	else if (strncmp(message, cmdGrid, 5) == 0) {
		auto selection = callback->GetSelectedUnits();
		if (!selection.empty()) {
			if (selection[0]->GetAllyTeam() == allyTeamId) {
				allyTeam->GetEnergyGrid()->ToggleVis();
			}
			utils::free_clear(selection);
		} else if (allyTeam->GetEnergyGrid()->IsVis()) {
			allyTeam->GetEnergyGrid()->ToggleVis();
		}
	}
	else if (strncmp(message, cmdNode, 5) == 0) {
		const AIFloat3 dbgPos = map->GetMousePos();
		economyManager->GetEnergyGrid()->DrawNodePylons(dbgPos);
	}
	else if (strncmp(message, cmdLink, 5) == 0) {
		const AIFloat3 dbgPos = map->GetMousePos();
		economyManager->GetEnergyGrid()->DrawLinkPylons(dbgPos);
	}

	else if (strncmp(message, cmdName, 5) == 0) {
		pathfinder->SetDbgDef(GetCircuitDef(message[6]));
		pathfinder->SetDbgPos(map->GetMousePos());
		const AIFloat3& dbgPos = pathfinder->GetDbgPos();
		LOG("%f, %f, %f, %i", dbgPos.x, dbgPos.y, dbgPos.z, pathfinder->GetDbgDef());
	}
	else if (strncmp(message, cmdEnd, 4) == 0) {
		PathInfo path;
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
//		if (unit->GetCircuitDef()->GetDef()->IsAbleToCloak()) {
//			unit->GetUnit()->ExecuteCustomCommand(CMD_WANT_CLOAK, {1.0f});  // personal
//			unit->GetUnit()->ExecuteCustomCommand(CMD_CLOAK_SHIELD, {1.0f});  // area
//			unit->GetUnit()->Cloak(true);
//		}
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

int CCircuitAI::UnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker/*, int weaponId*/)
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

int CCircuitAI::UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	for (auto& module : modules) {
		module->UnitDestroyed(unit, attacker);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitGiven(ICoreUnit::Id unitId, int oldTeamId, int newTeamId)
{
	CEnemyInfo* enemy = GetEnemyInfo(unitId);
	if (enemy != nullptr) {
		EnemyDestroyed(enemy);
		UnregisterEnemyInfo(enemy);
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
//		if (unit->GetCircuitDef()->GetDef()->IsAbleToCloak()) {
//			unit->GetUnit()->ExecuteCustomCommand(CMD_WANT_CLOAK, {1.0f});  // personal
//			unit->GetUnit()->ExecuteCustomCommand(CMD_CLOAK_SHIELD, {1.0f});  // area
//			unit->GetUnit()->Cloak(true);
//		}
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

int CCircuitAI::EnemyEnterLOS(CEnemyInfo* enemy)
{
	bool isSuddenThreat = mapManager->IsSuddenThreat(enemy->GetData());

	allyTeam->EnemyEnterLOS(enemy->GetData(), this);

	if (!isSuddenThreat) {
		return 0;  // signaling: OK
	}
	// Force unit's reaction
	auto friendlies = callback->GetFriendlyUnitIdsIn(enemy->GetPos(), 500.0f);
	if (friendlies.empty()) {
		return 0;  // signaling: OK
	}
	for (int fId : friendlies) {
		if (fId == -1) {
			continue;
		}
		CCircuitUnit* unit = GetTeamUnit(fId);
		if ((unit != nullptr) && (unit->GetTask() != nullptr)) {
			unit->ForceExecute(lastFrame + THREAT_UPDATE_RATE);
		}
	}

	return 0;  // signaling: OK
}

int CCircuitAI::EnemyLeaveLOS(CEnemyInfo* enemy)
{
	allyTeam->EnemyLeaveLOS(enemy->GetData(), this);

	return 0;  // signaling: OK
}

int CCircuitAI::EnemyEnterRadar(CEnemyInfo* enemy)
{
	allyTeam->EnemyEnterRadar(enemy->GetData(), this);

	return 0;  // signaling: OK
}

int CCircuitAI::EnemyLeaveRadar(CEnemyInfo* enemy)
{
	allyTeam->EnemyLeaveRadar(enemy->GetData(), this);

	return 0;  // signaling: OK
}

int CCircuitAI::EnemyDamaged(CEnemyInfo* enemy)
{
	// NOTE: Whole threat map updates in a fraction of a second, through polling
	return 0;  // signaling: OK
}

int CCircuitAI::EnemyDestroyed(CEnemyInfo* enemy)
{
	allyTeam->EnemyDestroyed(enemy->GetData(), this);

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
	isLoadSave = true;

	auto units = callback->GetTeamUnits();
	for (Unit* u : units) {
		if (u == nullptr) {
			continue;
		}
		ICoreUnit::Id unitId = u->GetUnitId();
		CCircuitUnit* unit = GetTeamUnit(unitId);
		if (unit != nullptr) {
			delete u;
			continue;
		}
		unit = RegisterTeamUnit(unitId, u);
		u->IsBeingBuilt() ? UnitCreated(unit, nullptr) : UnitFinished(unit);
	}
	for (auto& kv : teamUnits) {
		CCircuitUnit* unit = kv.second;
		if (unit->GetUnit()->GetRulesParamFloat("disableAiControl", 0) > 0.f) {
			DisableControl(unit);
		}
	}

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

	return RegisterTeamUnit(unitId, u);
}

CCircuitUnit* CCircuitAI::RegisterTeamUnit(ICoreUnit::Id unitId, Unit* u)
{
	CCircuitDef* cdef = GetCircuitDef(GetCallback()->Unit_GetDefId(unitId));
	CCircuitUnit* unit = new CCircuitUnit(unitId, u, cdef);

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

std::pair<CEnemyInfo*, bool> CCircuitAI::RegisterEnemyInfo(ICoreUnit::Id unitId, bool isInLOS)
{
	CEnemyInfo* unit = GetEnemyInfo(unitId);
	if (unit != nullptr) {
		if (isInLOS && !allyTeam->EnemyInLOS(unit->GetData(), this)) {
			return std::make_pair(nullptr, false);
		}
		return std::make_pair(unit, true);
	}

	CEnemyUnit* data;
	bool isReal;
	std::tie(data, isReal) = allyTeam->RegisterEnemyUnit(unitId, isInLOS, this);
	if (data == nullptr) {
		return std::make_pair(nullptr, isReal);
	}

	unit = new CEnemyInfo(data);
	enemyInfos[unit->GetId()] = unit;

	return std::make_pair(unit, true);
}

CEnemyInfo* CCircuitAI::RegisterEnemyInfo(Unit* e)
{
	CEnemyUnit* data = allyTeam->RegisterEnemyUnit(e, this);
	if (data == nullptr) {
		return nullptr;
	}

	CEnemyInfo* unit = new CEnemyInfo(data);
	enemyInfos[unit->GetId()] = unit;

	return unit;
}

void CCircuitAI::UnregisterEnemyInfo(CEnemyInfo* enemy)
{
	allyTeam->UnregisterEnemyUnit(enemy->GetData(), this);
	enemyInfos.erase(enemy->GetId());
	delete enemy;
}

CEnemyInfo* CCircuitAI::GetEnemyInfo(ICoreUnit::Id unitId) const
{
	auto it = enemyInfos.find(unitId);
	return (it != enemyInfos.end()) ? it->second : nullptr;
}

void CCircuitAI::DisableControl(CCircuitUnit* unit)
{
	if (unit->GetTask() != nullptr) {
		ITaskManager* mgr = unit->GetTask()->GetManager();
		mgr->AssignTask(unit, new CPlayerTask(mgr));
	}
}

void CCircuitAI::DisableControl(const std::string data)
{
	std::smatch section;
	std::string::const_iterator start = data.begin();
	std::string::const_iterator end = data.end();
	std::regex patternUnit("\\w+");
	while (std::regex_search(start, end, section, patternUnit)) {
		CCircuitUnit* unit = GetTeamUnit(utils::string_to_int(section[0]));
		if (unit != nullptr) {
			DisableControl(unit);
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

void CCircuitAI::UpdateActions()
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
				--n;
			}
			++actionIterator;
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

	if (!gameAttribute->IsInitialized()) {
		value = options->GetValueByKey("random_seed");
		unsigned int seed = (value != nullptr) ? StringToInt(value) : time(nullptr);
		gameAttribute->Init(seed);
	}

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
	auto unitDefs = callback->GetUnitDefs();
	for (UnitDef* ud : unitDefs) {
		auto options = ud->GetBuildOptions();
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

CWeaponDef* CCircuitAI::GetWeaponDef(CWeaponDef::Id weaponDefId) const
{
	return (size_t)weaponDefId < weaponDefs.size() ? weaponDefs[weaponDefId] : nullptr;
}

void CCircuitAI::InitWeaponDefs()
{
	auto weapDefs = callback->GetWeaponDefs();
	weaponDefs.reserve(weapDefs.size());
	for (WeaponDef* wd : weapDefs) {
		weaponDefs.push_back(new CWeaponDef(wd));
	}
}

CThreatMap* CCircuitAI::GetThreatMap() const
{
	return mapManager->GetThreatMap();
}

CInfluenceMap* CCircuitAI::GetInflMap() const
{
	return mapManager->GetInflMap();
}

void CCircuitAI::CreateGameAttribute()
{
	if (gameAttribute == nullptr) {
		gameAttribute = std::unique_ptr<CGameAttribute>(new CGameAttribute());
		CCircuitDef::InitStatic(this, &gameAttribute->GetRoleMasker());
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
