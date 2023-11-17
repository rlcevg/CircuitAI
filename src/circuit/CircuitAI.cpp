/*
 * Circuit.cpp
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#include "CircuitAI.h"
#include "scheduler/Scheduler.h"
#include "script/ScriptManager.h"
#include "script/InitScript.h"
#include "setup/SetupManager.h"
#include "map/MapManager.h"
#include "map/ThreatMap.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "module/EconomyManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/path/PathFinder.h"
#include "task/PlayerTask.h"
#include "unit/CircuitUnit.h"
#include "unit/enemy/EnemyUnit.h"
#include "util/GameAttribute.h"
#include "util/Utils.h"
#include "util/Profiler.h"
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
#include "Mod.h"
#include "Cheats.h"
//#include "WrappCurrentCommand.h"

#include <regex>
#include <fstream>

namespace circuit {

using namespace springai;
using namespace terrain;

#define ACTION_UPDATE_RATE	32
#define RELEASE_RESIGN		100
#define RELEASE_SIDE		200
#define RELEASE_CONFIG		201
#define RELEASE_SCRIPT		202
#define RELEASE_COMMANDER	203
#define RELEASE_CORRUPTED	204
#ifdef CIRCUIT_PROFILING
	#define TRACY_TOPIC(txt, topic)	\
		ZoneScopedN(txt);	\
		ZoneName(profiler.GetEvent ## topic ## Name(skirmishAIId), profiler.GetEvent ## topic ## Size(skirmishAIId))
	#define TRACY_TOPIC_UNIT(txt, topic, unit)	\
		TRACY_TOPIC(txt, topic);	\
		ZoneValue(unit)
#else
	#define TRACY_TOPIC(txt, topic)
	#define TRACY_TOPIC_UNIT(txt, topic, unit)
#endif

/*
 * Почему-то всегда
 * Так незыблемы цели:
 * Разрушать города,
 * Видеть в братьях мишени...
 */
constexpr char version[]{"1.6.9"};
constexpr uint32_t VERSION_SAVE = 4;

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
		, isSavegame(false)
		, isLoadSave(false)
		, isResigned(false)
		, isSlave(false)
		// NOTE: assert(lastFrame != -1): CCircuitUnit initialized with -1
		//       and lastFrame check will misbehave until first update event.
		, lastFrame(-2)
		, skirmishAIId(clb->GetSkirmishAIId())
		, sideId(0)
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
		, script(nullptr)
		, category({0})
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
	metalRes = callback->GetResourceByName(RES_NAME_METAL);
	energyRes = callback->GetResourceByName(RES_NAME_ENERGY);
	eventHandler = &CCircuitAI::HandleResignEvent;
}

void CCircuitAI::Resign(int newTeamId)
{
	std::vector<Unit*> migrants;
	auto allTeamUnits = callback->GetTeamUnits();
	for (Unit* u : allTeamUnits) {
		migrants.push_back(u);
	}
	economy->SendUnits(migrants, newTeamId);
	utils::free_clear(allTeamUnits);
	allyTeam->ForceUpdateFriendlyUnits();

	ownerTeamId = newTeamId;
	isResigned = true;
}

void CCircuitAI::MobileSlave(int newTeamId)
{
	std::vector<Unit*> migrants;
	std::vector<CCircuitUnit*> clean;
	for (auto& kv : teamUnits) {
		CCircuitUnit* unit = kv.second;
		// NOTE: springai::Economy::SendUnits won't send unfinished nanoframes,
		//       and it does cause issues when UnitFinished arrives
		//       but UnitDestroyed already cleaned its data.
		if (unit->GetCircuitDef()->IsMobile()
			&& !unit->GetCircuitDef()->IsRoleBuilder()
			&& !unit->GetUnit()->IsBeingBuilt())
		{
			migrants.push_back(unit->GetUnit());
			clean.push_back(unit);
		}
	}
	economy->SendUnits(migrants, newTeamId);
	// NOTE: How to check actually sent units? see note above why it matters.
	for (CCircuitUnit* unit : clean) {
		UnitDestroyed(unit, nullptr);
		UnregisterTeamUnit(unit);
	}
	allyTeam->ForceUpdateFriendlyUnits();

	ownerTeamId = newTeamId;
	isSlave = true;
}

int CCircuitAI::HandleGameEvent(int topic, const void* data)
{
	int ret = ERROR_UNKNOWN;

	switch (topic) {
		case EVENT_INIT: {
			TRACY_TOPIC("EVENT_INIT", Init);

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
			return ret;
		} break;
		case EVENT_RELEASE: {
			TRACY_TOPIC("EVENT_RELEASE", Release);

			struct SReleaseEvent* evt = (struct SReleaseEvent*)data;
			ret = this->Release(evt->reason);
		} break;
		case EVENT_UPDATE: {
			FrameMarkNamed(profiler.GetEventUpdateName(skirmishAIId));
			TRACY_TOPIC("EVENT_UPDATE", Update);

			struct SUpdateEvent* evt = (struct SUpdateEvent*)data;
			ret = this->Update(evt->frame);
		} break;
		case EVENT_MESSAGE: {
			TRACY_TOPIC("EVENT_MESSAGE", Message);

			struct SMessageEvent* evt = (struct SMessageEvent*)data;
			ret = this->Message(evt->player, evt->message);
		} break;
		case EVENT_UNIT_CREATED: {
			struct SUnitCreatedEvent* evt = (struct SUnitCreatedEvent*)data;
			TRACY_TOPIC_UNIT("EVENT_UNIT_CREATED", UnitCreated, evt->unit);

			CCircuitUnit* builder = GetTeamUnit(evt->builder);
			CCircuitUnit* unit = GetOrRegTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitCreated(unit, builder) : ERROR_UNIT_CREATED;
		} break;
		case EVENT_UNIT_FINISHED: {
			struct SUnitFinishedEvent* evt = (struct SUnitFinishedEvent*)data;
			TRACY_TOPIC_UNIT("EVENT_UNIT_FINISHED", UnitFinished, evt->unit);

			// Lua might call SetUnitHealth within eventHandler.UnitCreated(this, builder);
			// and trigger UnitFinished before eoh->UnitCreated(*this, builder);
			// @see rts/Sim/Units/Unit.cpp CUnit::PostInit
			CCircuitUnit* unit = GetOrRegTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitFinished(unit) : ERROR_UNIT_FINISHED;
		} break;
		case EVENT_UNIT_IDLE: {
			struct SUnitIdleEvent* evt = (struct SUnitIdleEvent*)data;
			TRACY_TOPIC_UNIT("EVENT_UNIT_IDLE", UnitIdle, evt->unit);

			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitIdle(unit) : ERROR_UNIT_IDLE;
		} break;
		case EVENT_UNIT_MOVE_FAILED: {
			struct SUnitMoveFailedEvent* evt = (struct SUnitMoveFailedEvent*)data;
			TRACY_TOPIC_UNIT("EVENT_UNIT_MOVE_FAILED", UnitMoveFailed, evt->unit);

			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitMoveFailed(unit) : ERROR_UNIT_MOVE_FAILED;
		} break;
		case EVENT_UNIT_DAMAGED: {
			struct SUnitDamagedEvent* evt = (struct SUnitDamagedEvent*)data;
			TRACY_TOPIC_UNIT("EVENT_UNIT_DAMAGED", UnitDamaged, evt->unit);

			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			ret = (unit != nullptr)
					? this->UnitDamaged(unit, evt->attacker, evt->weaponDefId, AIFloat3(evt->dir_posF3))
					: ERROR_UNIT_DAMAGED;
		} break;
		case EVENT_UNIT_DESTROYED: {
			struct SUnitDestroyedEvent* evt = (struct SUnitDestroyedEvent*)data;
			TRACY_TOPIC_UNIT("EVENT_UNIT_DESTROYED", UnitDestroyed, evt->unit);

			CEnemyInfo* attacker = GetEnemyInfo(evt->attacker);
			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			if (unit != nullptr) {
				ret = this->UnitDestroyed(unit, attacker);
				UnregisterTeamUnit(unit);
			} else {
				ret = ERROR_UNIT_DESTROYED;
			}
		} break;
		case EVENT_UNIT_GIVEN: {
			struct SUnitGivenEvent* evt = (struct SUnitGivenEvent*)data;
			TRACY_TOPIC_UNIT("EVENT_UNIT_GIVEN", UnitGiven, evt->unitId);

			ret = this->UnitGiven(evt->unitId, evt->oldTeamId, evt->newTeamId);
		} break;
		case EVENT_UNIT_CAPTURED: {
			struct SUnitCapturedEvent* evt = (struct SUnitCapturedEvent*)data;
			TRACY_TOPIC_UNIT("EVENT_UNIT_CAPTURED", UnitCaptured, evt->unitId);

			ret = this->UnitCaptured(evt->unitId, evt->oldTeamId, evt->newTeamId);
		} break;
		case EVENT_ENEMY_ENTER_LOS: {
			TRACY_TOPIC("EVENT_ENEMY_ENTER_LOS", EnemyEnterLOS);

			struct SEnemyEnterLOSEvent* evt = (struct SEnemyEnterLOSEvent*)data;
			CEnemyInfo* enemy;
			bool isReal;
			std::tie(enemy, isReal) = RegisterEnemyInfo(evt->enemy, true);
			ret = isReal
					? (enemy != nullptr) ? this->EnemyEnterLOS(enemy) : ERROR_ENEMY_ENTER_LOS
					: 0;
		} break;
		case EVENT_ENEMY_LEAVE_LOS: {
			TRACY_TOPIC("EVENT_ENEMY_LEAVE_LOS", EnemyLeaveLOS);

			if (isCheating) {
				ret = 0;
			} else {
				struct SEnemyLeaveLOSEvent* evt = (struct SEnemyLeaveLOSEvent*)data;
				CEnemyInfo* enemy = GetEnemyInfo(evt->enemy);
				ret = (enemy != nullptr) ? this->EnemyLeaveLOS(enemy) : ERROR_ENEMY_LEAVE_LOS;
			}
		} break;
		case EVENT_ENEMY_ENTER_RADAR: {
			TRACY_TOPIC("EVENT_ENEMY_ENTER_RADAR", EnemyEnterRadar);

			struct SEnemyEnterRadarEvent* evt = (struct SEnemyEnterRadarEvent*)data;
			CEnemyInfo* enemy;
			bool isReal;
			std::tie(enemy, isReal) = RegisterEnemyInfo(evt->enemy, false);
			ret = isReal
					? (enemy != nullptr) ? this->EnemyEnterRadar(enemy) : ERROR_ENEMY_ENTER_RADAR
					: 0;
		} break;
		case EVENT_ENEMY_LEAVE_RADAR: {
			TRACY_TOPIC("EVENT_ENEMY_LEAVE_RADAR", EnemyLeaveRadar);

			if (isCheating) {
				ret = 0;
			} else {
				struct SEnemyLeaveRadarEvent* evt = (struct SEnemyLeaveRadarEvent*)data;
				CEnemyInfo* enemy = GetEnemyInfo(evt->enemy);
				ret = (enemy != nullptr) ? this->EnemyLeaveRadar(enemy) : ERROR_ENEMY_LEAVE_RADAR;
			}
		} break;
		case EVENT_ENEMY_DAMAGED: {
			TRACY_TOPIC("EVENT_ENEMY_DAMAGED", EnemyDamaged);

			struct SEnemyDamagedEvent* evt = (struct SEnemyDamagedEvent*)data;
			CEnemyInfo* enemy = GetEnemyInfo(evt->enemy);
			ret = (enemy != nullptr) ? this->EnemyDamaged(enemy) : ERROR_ENEMY_DAMAGED;
		} break;
		case EVENT_ENEMY_DESTROYED: {
			TRACY_TOPIC("EVENT_ENEMY_DESTROYED", EnemyDestroyed);

			struct SEnemyDestroyedEvent* evt = (struct SEnemyDestroyedEvent*)data;
			CEnemyInfo* enemy = GetEnemyInfo(evt->enemy);
			if (enemy != nullptr) {
				allyTeam->DyingEnemy(enemy->GetData(), lastFrame);
				ret = 0;
			} else {
				ret = ERROR_ENEMY_DESTROYED;
			}
		} break;
		case EVENT_WEAPON_FIRED: {
			TRACY_TOPIC("EVENT_WEAPON_FIRED", WeaponFired);

			ret = 0;
		} break;
		case EVENT_PLAYER_COMMAND: {
			TRACY_TOPIC("EVENT_PLAYER_COMMAND", PlayerCommand);

			struct SPlayerCommandEvent* evt = (struct SPlayerCommandEvent*)data;
			std::vector<CCircuitUnit*> units;
			units.reserve(evt->unitIds_size);
			for (int i = 0; i < evt->unitIds_size; i++) {
				units.push_back(GetTeamUnit(evt->unitIds[i]));
			}
			ret = this->PlayerCommand(units);
		} break;
		case EVENT_SEISMIC_PING: {
			TRACY_TOPIC("EVENT_SEISMIC_PING", SeismicPing);

			ret = 0;
		} break;
		case EVENT_COMMAND_FINISHED: {
			TRACY_TOPIC("EVENT_COMMAND_FINISHED", CommandFinished);

			// FIXME: commandId always == -1, no use
//			struct SCommandFinishedEvent* evt = (struct SCommandFinishedEvent*)data;
//			CCircuitUnit* unit = GetTeamUnit(evt->unitId);
//			springai::Command* command = WrappCurrentCommand::GetInstance(skirmishAIId, evt->unitId, evt->commandId);
//			this->CommandFinished(unit, evt->commandTopicId, command);
//			delete command;
			ret = 0;
		} break;
		case EVENT_LOAD: {
			TRACY_TOPIC("EVENT_LOAD", Load);

			struct SLoadEvent* evt = (struct SLoadEvent*)data;
			std::ifstream loadFileStream;
			loadFileStream.open(evt->file, std::ios::binary);
			ret = loadFileStream.is_open() ? this->Load(loadFileStream) : ERROR_LOAD;
			loadFileStream.close();
			return ret;
		} break;
		case EVENT_SAVE: {
			TRACY_TOPIC("EVENT_SAVE", Save);

			struct SSaveEvent* evt = (struct SSaveEvent*)data;
			std::ofstream saveFileStream;
			saveFileStream.open(evt->file, std::ios::binary);
			ret = saveFileStream.is_open() ? this->Save(saveFileStream) : ERROR_SAVE;
			saveFileStream.close();
			return ret;
		} break;
		case EVENT_ENEMY_CREATED: {
			TRACY_TOPIC("EVENT_ENEMY_CREATED", EnemyCreated);

			// @see Cheats::SetEventsEnabled
			// FIXME: Can't query enemy data with globalLOS
			struct SEnemyCreatedEvent* evt = (struct SEnemyCreatedEvent*)data;
			CEnemyInfo* unit;
			bool isReal;
			std::tie(unit, isReal) = RegisterEnemyInfo(evt->enemy, true);
			ret = isReal
					? (unit != nullptr) ? this->EnemyEnterLOS(unit) : EVENT_ENEMY_CREATED
					: 0;
		} break;
		case EVENT_ENEMY_FINISHED: {
			TRACY_TOPIC("EVENT_ENEMY_FINISHED", EnemyFinished);

			// @see Cheats::SetEventsEnabled
			ret = 0;
		} break;
		case EVENT_LUA_MESSAGE: {
			TRACY_TOPIC("EVENT_LUA_MESSAGE", LuaMessage);

			struct SLuaMessageEvent* evt = (struct SLuaMessageEvent*)data;
			ret = this->LuaMessage(evt->inData);
		} break;
		default: {
			LOG("%i WARNING unrecognized event: %i", skirmishAIId, topic);
			ret = 0;
		} break;
	}

#ifndef DEBUG_LOG
	ret = 0;
#endif
	return ret;
}

int CCircuitAI::HandleEndEvent(int topic, const void* data)
{
	if (topic == EVENT_RELEASE) {
		TRACY_TOPIC("EVENT_RELEASE::END", ReleaseEnd);

		struct SReleaseEvent* evt = (struct SReleaseEvent*)data;
		return this->Release(evt->reason);
	}
	return 0;
}

int CCircuitAI::HandleResignEvent(int topic, const void* data)
{
	switch (topic) {
		case EVENT_RELEASE: {
			TRACY_TOPIC("EVENT_RELEASE::RESIGN", ReleaseResign);

			struct SReleaseEvent* evt = (struct SReleaseEvent*)data;
			return this->Release(evt->reason);
		} break;
		case EVENT_UPDATE: {
			FrameMarkNamed(profiler.GetEventUpdateName(skirmishAIId));
			TRACY_TOPIC("EVENT_UPDATE::RESIGN", UpdateResign);

			struct SUpdateEvent* evt = (struct SUpdateEvent*)data;
			if (evt->frame % (TEAM_SLOWUPDATE_RATE * INCOME_SAMPLES) == 0) {
				const int mId = metalRes->GetResourceId();
				const int eId = energyRes->GetResourceId();
				float m = game->GetTeamResourceStorage(ownerTeamId, mId) - HIDDEN_STORAGE - game->GetTeamResourceCurrent(ownerTeamId, mId);
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

std::string CCircuitAI::ValidateMod()
{
	const int minEngineVer = 105;
	const char* engineVersion = engine->GetVersionMajor();
	int ver = atoi(engineVersion);
	if (ver < minEngineVer) {
		LOG("Engine must be %i or higher! (Current: %s)", minEngineVer, engineVersion);
		return "";
	}

	Mod* mod = callback->GetMod();
	const char* name = mod->GetShortName();
	delete mod;
	if (name == nullptr) {
		LOG("Can't get name of the game. Aborting!");  // NOTE: Sign of messed up spring/AI installation
		return "";
	}

	return name;
}

void CCircuitAI::CheatPreload()
{
	auto& enemies = callback->GetEnemyUnits();
	for (Unit* e : enemies) {
		CEnemyInfo* enemy = RegisterEnemyInfo(e);
		if (enemy != nullptr) {
			this->EnemyEnterLOS(enemy);
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
	const std::string modName = ValidateMod();
	if (modName.empty()) {
		return ERROR_INIT;
	}

#ifdef DEBUG_VIS
	debugDrawer = std::unique_ptr<CDebugDrawer>(new CDebugDrawer(this, sAICallback));
	if (debugDrawer->Init() != 0) {
		return ERROR_INIT;
	}
#endif

	CreateGameAttribute();
	scheduler = std::make_shared<CScheduler>();
	scheduler->Init(scheduler);

	InitRoles();  // core c++ implemented roles
	const std::string profile = InitOptions();  // Inits GameAttribute
	scriptManager = std::make_shared<CScriptManager>(this);
	script = new CInitScript(GetScriptManager(), this);
	std::vector<std::string> cfgParts;
	CCircuitDef::SArmorInfo armor;
	if (!script->InitConfig(profile, cfgParts, armor)) {
		Release(RELEASE_SCRIPT);
		return ERROR_INIT;
	}

	if (!InitSide()) {
		Release(RELEASE_SIDE);
		return ERROR_INIT;
	}

	InitWeaponDefs();
	float decloakRadius;
	InitUnitDefs(armor, decloakRadius);  // Inits TerrainData

	setupManager = std::make_shared<CSetupManager>(this, &gameAttribute->GetSetupData());
	if (!setupManager->OpenConfig(profile, cfgParts)) {
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
	economy = callback->GetEconomy();

	allyTeam->Init(this, decloakRadius);
	mapManager = allyTeam->GetMapManager();
	enemyManager = allyTeam->GetEnemyManager();
	metalManager = allyTeam->GetMetalManager();
	energyManager = allyTeam->GetEnergyManager();
	pathfinder = allyTeam->GetPathfinder();

	// FIXME: CanChooseStartPos = false, finish start factory and position selection
	if (setupManager->HasStartBoxes() && setupManager->CanChooseStartPos()) {
		const CSetupManager::StartPosType spt = metalManager->HasMetalSpots() ?
												CSetupManager::StartPosType::METAL_SPOT :
												CSetupManager::StartPosType::RANDOM;
		setupManager->PickStartPos(spt);
	}

	factoryManager = std::make_shared<CFactoryManager>(this);
	builderManager = std::make_shared<CBuilderManager>(this);
	militaryManager = std::make_shared<CMilitaryManager>(this);

	// TODO: Remove EconomyManager from module (move abilities to BuilderManager).
	modules.push_back(militaryManager);
	modules.push_back(builderManager);
	modules.push_back(factoryManager);  // NOTE: Contains special last-module unit handlers.
	modules.push_back(economyManager);  // NOTE: Uses unit's manager != nullptr, thus must be last.

	terrainManager->Init();

	script->RegisterMgr();
	if (!script->Init()) {
		Release(RELEASE_SCRIPT);
		return ERROR_INIT;
	}
	for (auto& module : modules) {
		if (!module->InitScript()) {
			Release(RELEASE_SCRIPT);
			return ERROR_INIT;
		}
	}

	if (isCheating) {
		cheats->SetEnabled(true);
		cheats->SetEventsEnabled(true);
		scheduler->RunJobAt(CScheduler::GameJob(&CCircuitAI::CheatPreload, this), skirmishAIId + 1);
	}

	if (isCommMerge) {
		if ((GetEnemyTeamSize() < allyTeam->GetAliveSize() / 2.f)/* || (allyTeam->GetAliveSize() > 4)*/) {
			mergeTask = CScheduler::GameJob([this] {
#if 1
				if (allyTeam->GetLeaderId() == teamId) {
					scheduler->RemoveJob(mergeTask);
				} else if (factoryManager->GetFactoryCount() > 0) {
					MobileSlave(allyTeam->GetLeaderId());
					scheduler->RemoveJob(mergeTask);
				}
#else
				// Complete resign by area
				if (allyTeam->GetLeaderId() == teamId) {
					scheduler->RemoveJob(mergeTask);
				} else if (factoryManager->GetFactoryCount() > 0) {
					CCircuitUnit* commander = setupManager->GetCommander();
					if (commander == nullptr) {
						commander = teamUnits.begin()->second;
					}
					int ownerId = allyTeam->GetAreaTeam(commander->GetArea()).teamId;
					if ((ownerId != teamId) && (ownerId >= 0)) {
						Resign(ownerId);
					}
				}
#endif
			});
			scheduler->RunJobEvery(mergeTask, FRAMES_PER_SEC, FRAMES_PER_SEC * 10);
#if 0
		} else if (allyTeam->GetAliveSize() > 2) {
			mergeTask = CScheduler::GameJob([this] {
				if (allyTeam->GetLeaderId() == teamId) {
					scheduler->RemoveJob(mergeTask);
				} else if (factoryManager->GetNoT1FacCount() > 0) {
					MobileSlave(allyTeam->GetLeaderId());
					scheduler->RemoveJob(mergeTask);
				}
			});
			scheduler->RunJobEvery(mergeTask, FRAMES_PER_SEC, FRAMES_PER_SEC * 10);
#endif
		}
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

	if (!isInitialized && (reason < RELEASE_SIDE)) {
		return 0;
	}

	scheduler->ProcessRelease();
	scheduler = nullptr;

	delete script;  // NOTE: Threaded scripts, hence destroy contexts after scheduler
	script = nullptr;

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

	weaponDefs.clear();
	defsById.clear();
	defsByName.clear();

	modules.clear();
	scriptManager = nullptr;
	militaryManager = nullptr;
	economyManager = nullptr;
	factoryManager = nullptr;
	builderManager = nullptr;
	terrainManager = nullptr;
	metalManager = nullptr;
	energyManager = nullptr;
	pathfinder = nullptr;
	setupManager = nullptr;
	enemyManager = nullptr;
	mapManager = nullptr;

	for (CCircuitUnit* unit : actionUnits) {
		if (unit->IsDead()) {  // instance is not in teamUnits
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

	for (const CEnemyUnit* data : allyTeam->GetDyingEnemies()) {
		CEnemyInfo* enemy = GetEnemyInfo(data->GetId());
		if (enemy != nullptr) {  // EnemyDestroyed right after UpdateEnemyDatas but before this Update
			EnemyDestroyed(enemy);
			UnregisterEnemyInfo(enemy);
		}
	}

	allyTeam->Update(this);

	scheduler->ProcessJobs(frame);
	if (frame % TEAM_SLOWUPDATE_RATE == skirmishAIId) {
		// NOTE: Probably should be last in ProcessJobs queue, after all income updates if it was in the same frame.
		//       Hence it is not:
		// scheduler->RunJobEvery(CScheduler::GameJob(&CInitScript::Update, script), TEAM_SLOWUPDATE_RATE, skirmishAIId);
		script->Update();
	}
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
	const char cmdBreak[]   = "~break";
	const char cmdReload[]  = "~reload";

	const char cmdPos[]     = "~стройсь\0";
	const char cmdSelfD[]   = "~Згинь, нечистая сила!\0";

	const char cmdBlock[]   = "~block";
	const char cmdWBlock[]  = "~wbdraw";  // widget block draw

	const char cmdArea[]    = "~area";
	const char cmdPath[]    = "~path";
	const char cmdKnn[]     = "~knn";
	const char cmdLog[]     = "~log";
	const char cmdBTask[]   = "~btask";
	const char cmdChoke[]   = "~choke";
	const char cmdMetal[]   = "~metal";

	const char cmdThreat[]  = "~threat";
	const char cmdWTDraw[]  = "~wtdraw";  // widget threat draw
	const char cmdWTDiv[]   = "~wtdiv";
	const char cmdWTPrint[] = "~wtprint";

	const char cmdInfl[]    = "~infl";
	const char cmdWIDraw[]  = "~widraw";  // widget influence draw
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
			u->SelfDestruct();
			delete u;
		}
	};

	size_t msgLength = strlen(message);

	if (strncmp(message, cmdBreak, 6) == 0) {
		__asm__("int3");
	}
	else if (strncmp(message, cmdReload, 7) == 0) {
		game->SetPause(true, "reload");
		scriptManager->Reload();
	}

	else if ((msgLength == strlen(cmdPos)) && (strcmp(message, cmdPos) == 0)) {
		setupManager->PickStartPos(CSetupManager::StartPosType::RANDOM);
	}
	else if ((msgLength == strlen(cmdSelfD)) && (strcmp(message, cmdSelfD) == 0)) {
		selfD();
	}

	else if (strncmp(message, cmdBlock, 6) == 0) {
		terrainManager->ToggleVis();
	}
	else if (strncmp(message, cmdWBlock, 7) == 0) {
		if (teamId == atoi((const char*)&message[8])) {
			terrainManager->ToggleWidgetDraw();
		}
	}

	else if (strncmp(message, cmdArea, 5) == 0) {
		gameAttribute->GetTerrainData().ToggleVis(lastFrame);
	}
	else if (strncmp(message, cmdPath, 5) == 0) {
		pathfinder->ToggleVis(this);
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
	else if (strncmp(message, cmdBTask, 6) == 0) {
		if (teamId == atoi((const char*)&message[7])) {
			builderManager->Log();
		}
	}
	else if (strncmp(message, cmdChoke, 6) == 0) {
		gameAttribute->GetTerrainData().ToggleTAVis(lastFrame);
	}
	else if (strncmp(message, cmdMetal, 6) == 0) {
		gameAttribute->GetMetalData().ToggleTAVis(lastFrame);
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
		std::string s(message);
		auto start = s.rfind(" ");
		std::string layer = (start != std::string::npos) ? s.substr(start + 1) : "";
		mapManager->GetThreatMap()->SetMaxThreat(atof((const char*)&message[7]), layer);
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
		pathfinder->SetDbgType(atoi((const char*)&message[5]));
		AIFloat3 endPos = map->GetMousePos();
		std::shared_ptr<IPathQuery> query = pathfinder->CreateDbgPathQuery(GetThreatMap(),
				endPos, pathfinder->GetSquareSize());
		if (query != nullptr) {
			pathfinder->SetDbgQuery(query);
			pathfinder->RunQuery(scheduler.get(), query);
		}
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

	// NOTE: "response" structure and limits are per AI
	if (isSlave
		&& unit->GetCircuitDef()->IsMobile()
		&& !unit->GetCircuitDef()->IsRoleBuilder())
	{
		economy->SendUnits({unit->GetUnit()}, allyTeam->GetLeaderId());
		if (unit->GetTask() != nullptr) {  // NOTE: Won't send nanoframes but UnregisterTeamUnit may be already called.
			UnitDestroyed(unit, nullptr);
		}
		// BAR on SendUnits invokes EVENT_UNIT_CAPTURED, no need for:
		UnregisterTeamUnit(unit);
		return 0;
	}

	// FIXME: Random-Side workaround
	// Faction data used before UnitFinished reaches EconomyManager where it sets side if commander is null.
	// Option: remove faction specific lists and make common by economy type list with all water/underwater/factions units.
	// Cons: iterating over full list.
	if (unit->GetCircuitDef()->IsRoleComm() && (setupManager->GetCommander() == nullptr)) {
		setupManager->SetCommander(unit);
	}

	unit->GetCircuitDef()->AdjustSinceFrame(lastFrame);
	TRY_UNIT(this, unit,
		unit->CmdFireAtRadar(true);
		unit->GetUnit()->SetAutoRepairLevel(0);
		unit->GetUnit()->SetOn(unit->GetCircuitDef()->IsOn());
		if (unit->GetCircuitDef()->IsAbleToCloak()
			&& unit->GetCircuitDef()->GetCloakCost() < economyManager->GetAvgEnergyIncome() * 0.1f)
		{
			unit->CmdCloak(true);
		}
	)

	for (auto& module : modules) {
		module->UnitFinished(unit);
	}

	if ((unit->GetTask()->GetType() != IUnitTask::Type::NIL)
		&& (unit->GetUnit()->GetRulesParamFloat("resurrected", 0.f) != 0.f))
	{
		unit->GetTask()->GetManager()->Resurrected(unit);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitIdle(CCircuitUnit* unit)
{
	if (unit->IsStuck()) {
		return 0;  // signaling: OK
	}

	for (auto& module : modules) {
		module->UnitIdle(unit);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitMoveFailed(CCircuitUnit* unit)
{
	if (unit->IsStuck()) {
		return 0;  // signaling: OK
	}

	if (unit->IsMoveFailed(lastFrame)) {
		TRY_UNIT(this, unit,
			unit->CmdStop();
			unit->GetUnit()->SetMoveState(2);
		)
//		Garbage(unit, "stuck");
		GetBuilderManager()->Enqueue(TaskB::Reclaim(IBuilderTask::Priority::NORMAL, unit));
	} else if (unit->GetTask()->GetType() != IUnitTask::Type::NIL) {
		unit->GetTask()->OnUnitMoveFailed(unit);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitDamaged(CCircuitUnit* unit, ICoreUnit::Id attackerId, int weaponId, AIFloat3 dir)
{
	unit->SetDamagedFrame(lastFrame);
	CEnemyInfo* attacker = GetEnemyInfo(attackerId);

	if (IsValidWeaponDefId(weaponId)) {
		if (attacker != nullptr) {
			CheckDecoy(attacker, weaponId);
		} else if ((dir != ZeroVector) && (GetFriendlyUnit(attackerId) == nullptr)) {
			CreateFakeEnemy(weaponId, unit->GetPos(lastFrame), dir);  // currently only for threatmap
		}
	}

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
		allyTeam->DyingEnemy(enemy->GetData(), lastFrame);
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
		unit->CmdStop();
		unit->CmdFireAtRadar(true);
		unit->GetUnit()->SetAutoRepairLevel(0);
		unit->GetUnit()->SetOn(true);
		if (unit->GetCircuitDef()->IsAbleToCloak()
			&& unit->GetCircuitDef()->GetCloakCost() < economyManager->GetAvgEnergyIncome() * 0.1f)
		{
			unit->CmdCloak(true);
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

int CCircuitAI::EnemyEnterLOS(CEnemyInfo* enemy)
{
	bool isSuddenThreat = mapManager->IsSuddenThreat(enemy->GetData());

	allyTeam->EnemyEnterLOS(enemy->GetData(), this);

	if (!isSuddenThreat) {
		return 0;  // signaling: OK
	}
	// Force unit's reaction
	auto& friendlies = callback->GetFriendlyUnitIdsIn(enemy->GetPos(), 1000.0f);
	if (friendlies.empty()) {
		return 0;  // signaling: OK
	}
	for (int fId : friendlies) {
		CCircuitUnit* unit = GetTeamUnit(fId);
		if ((unit != nullptr) && (unit->GetTask()->GetType() != IUnitTask::Type::NIL)) {
			unit->ForceUpdate(lastFrame + THREAT_UPDATE_RATE);
		}
	}

	militaryManager->AddPointOfInterest(enemy);

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

	militaryManager->DelPointOfInterest(enemy);

	return 0;  // signaling: OK
}

int CCircuitAI::PlayerCommand(const std::vector<CCircuitUnit*>& units)
{
	for (CCircuitUnit* unit : units) {
		if ((unit != nullptr)
			&& (unit->GetTask()->GetType() != IUnitTask::Type::NIL)  // ignore orders to nanoframes
			&& (unit->GetTask()->GetType() != IUnitTask::Type::PLAYER))
		{
			unit->GetTask()->GetManager()->AssignPlayerTask(unit);
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
	isSavegame = true;
	isLoadSave = true;

//	if (mergeTask != nullptr) {
//		scheduler->RemoveJob(mergeTask);
//	}

	uint32_t versionLoad;
	utils::binary_read(is, versionLoad);
	if (versionLoad != VERSION_SAVE) {
		return ERROR_LOAD;
	}
	utils::binary_read(is, lastFrame);
	utils::binary_read(is, sideId);

	auto units = callback->GetTeamUnits();
	for (Unit* u : units) {
		ICoreUnit::Id unitId = u->GetUnitId();
		if (GetTeamUnit(unitId) != nullptr) {
			delete u;
			continue;
		}
		CCircuitUnit* unit = RegisterTeamUnit(unitId, u);
		UnitCreated(unit, nullptr);  // NOTE: NIL task assigned only on UnitCreated
		if (!u->IsBeingBuilt()) {
			UnitFinished(unit);
		}
	}
	for (auto& kv : teamUnits) {
		CCircuitUnit* unit = kv.second;
		if (unit->GetUnit()->GetRulesParamFloat("disableAiControl", 0) > 0.f) {
			DisableControl(unit);
		}
	}

	auto& enemies = callback->GetEnemyUnits();
	for (Unit* e : enemies) {
		if (GetEnemyInfo(e->GetUnitId()) != nullptr) {
			delete e;
			continue;
		}
		CEnemyInfo* enemy = RegisterEnemyInfo(e);
		if (enemy != nullptr) {
			EnemyEnterRadar(enemy);
			if (enemy->GetCircuitDef() != nullptr) {
				EnemyEnterLOS(enemy);
			}
		}
	}
#ifdef DEBUG_SAVELOAD
	LOG("%s | versionLoad=%i | lastFrame=%i | sideId=%i | defs=%i | units=%i | enemies=%i", __PRETTY_FUNCTION__,
			versionLoad, lastFrame, sideId, GetCircuitDefs().size(), teamUnits.size(), enemyInfos.size());
#endif

	for (auto& module : modules) {
		is >> *module;
	}
	for (auto& module : modules) {
		module->LoadScript(is);
	}

	isLoadSave = false;
	return 0;  // signaling: OK
}

int CCircuitAI::Save(std::ostream& os)
{
	utils::binary_write(os, VERSION_SAVE);
	utils::binary_write(os, lastFrame);
	utils::binary_write(os, sideId);
#ifdef DEBUG_SAVELOAD
	LOG("%s | VERSION_SAVE=%i | lastFrame=%i | sideId=%i | defs=%i", __PRETTY_FUNCTION__, VERSION_SAVE, lastFrame, sideId, GetCircuitDefs().size());
#endif

	for (auto& module : modules) {
		os << *module;
	}
	for (auto& module : modules) {
		module->SaveScript(os);
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

bool CCircuitAI::InitSide()
{
	sideName = game->GetTeamSide(teamId);
	if (!gameAttribute->GetSideMasker().HasType(sideName)) {
		sideName = gameAttribute->GetSideMasker().GetName(0);
		if (sideName.empty()) {
			return false;
		}
	}
	sideId = gameAttribute->GetSideMasker().GetType(sideName);
	return true;
}

void CCircuitAI::SetSide(const std::string& name)
{
	const CMaskHandler::MaskName& masks = gameAttribute->GetSideMasker().GetMasks();
	auto it = masks.find(name);
	if (it != masks.end()) {
		sideName = name;
		sideId = it->second.type;
	}
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
	CCircuitUnit* unit = new CCircuitUnit(this, unitId, u, cdef);

	SArea* area;
	bool isValid;
	std::tie(area, isValid) = terrainManager->GetCurrentMapArea(cdef, unit->GetPos(lastFrame));
	unit->SetArea(area);

	teamUnits[unitId] = unit;
	cdef->Inc();

	// FIXME: Sometimes area where factory is placed is not suitable for its units.
	//        There Garbage() can cause infinite start-cancel loop.
//	if (!isValid) {
//		Garbage(unit, "useless");
//	}
	return unit;
}

void CCircuitAI::UnregisterTeamUnit(CCircuitUnit* unit)
{
	teamUnits.erase(unit->GetId());
	unit->GetCircuitDef()->Dec();

	/*(unit->GetTask() == nullptr) ? DeleteTeamUnit(unit) : */unit->SetIsDead();
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
	enemyInfos[unitId] = unit;

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

void CCircuitAI::CreateFakeEnemy(int weaponId, const AIFloat3& startPos, const AIFloat3& dir)
{
	const SWeaponToUnitDef& wuDef = weaponToUnitDefs[weaponId];
	if (wuDef.ids.empty()) {
		return;
	}
	float range = weaponDefs[weaponId].GetRange();
	const AIFloat3 enemyPos = CTerrainManager::CorrectPosition(startPos, dir, range);  // range adjusted
	CEnemyUnit* enemy = allyTeam->GetEnemyOrFakeIn(startPos, dir, range, enemyPos, range * 0.2f, wuDef.ids);
	if (enemy == nullptr) {
		int timeout = lastFrame;
		CCircuitDef::Id defId;
		if (wuDef.mobileIds.empty()) {  // static
			timeout += FRAMES_PER_SEC * 60 * 20;
			defId = *wuDef.staticIds.begin();
		} else {
			timeout += FRAMES_PER_SEC * 60 * 1;
			defId = *wuDef.mobileIds.begin();
		}
		allyTeam->RegisterEnemyFake(defId, enemyPos, timeout);
	} else if (enemy->IsBeingBuilt()) {
		enemy->SetBeingBuilt(false);
		enemy->SetHealth(enemy->GetCircuitDef()->GetHealth());
		GetThreatMap()->SetEnemyUnitThreat(enemy);
	}
}

void CCircuitAI::CheckDecoy(CEnemyInfo* enemy, int weaponId)
{
	CCircuitDef* edef = enemy->GetCircuitDef();
	if ((edef != nullptr) && edef->IsDecoy()) {
		const SWeaponToUnitDef& wuDef = weaponToUnitDefs[weaponId];
		if (!wuDef.ids.empty()) {
			allyTeam->UpdateInLOS(enemy->GetData(), *wuDef.ids.begin());
		}
	}
}

CEnemyInfo* CCircuitAI::GetEnemyInfo(ICoreUnit::Id unitId) const
{
	auto it = enemyInfos.find(unitId);
	return (it != enemyInfos.end()) ? it->second : nullptr;
}

void CCircuitAI::DisableControl(CCircuitUnit* unit)
{
//	if (unit->GetTask()->GetType() != IUnitTask::Type::NIL) {
		IUnitModule* mgr = unit->GetTask()->GetManager();
		mgr->AssignTask(unit, new CPlayerTask(mgr));
//	}
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
		if ((unit != nullptr)/* && (unit->GetTask()->GetType() != IUnitTask::Type::NIL)*/) {
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

	value = options->GetValueByKey("comm_merge");
	if (value != nullptr) {
		isCommMerge = StringToBool(value);
	}

	if (!gameAttribute->IsInitialized()) {
		value = options->GetValueByKey("random_seed");
		unsigned int seed = (value != nullptr) ? StringToInt(value) : time(nullptr);
		gameAttribute->Init(seed);
	}

	value = options->GetValueByKey("profile");
	std::string profile = ((value != nullptr) && strlen(value) > 0) ? value : "";

	delete options;
	return profile;
}

CCircuitDef* CCircuitAI::GetCircuitDef(const char* name)
{
	auto it = defsByName.find(name);
	// NOTE: For the sake of AI's health it should not return nullptr
	return (it != defsByName.end()) ? it->second : nullptr;
}

void CCircuitAI::InitRoles()
{
	for (const auto& kv : CCircuitDef::GetRoleNames()) {
		BindRole(kv.second.type, kv.second.type);
	}
}

void CCircuitAI::InitUnitDefs(const CCircuitDef::SArmorInfo& armor, float& outDcr)
{
	gameAttribute->GetTerrainData().Init(this);

	Resource* resM = callback->GetResourceByName(RES_NAME_METAL);
	Resource* resE = callback->GetResourceByName(RES_NAME_ENERGY);
	outDcr = 0.f;

	auto unitDefs = callback->GetUnitDefs();
	defsById.reserve(unitDefs.size());

	for (UnitDef* ud : unitDefs) {
		auto options = ud->GetBuildOptions();
		std::unordered_set<CCircuitDef::Id> opts;
		for (UnitDef* buildDef : options) {
			opts.insert(buildDef->GetUnitDefId());
			delete buildDef;
		}
		// new CCircuitDef(this, ud, opts, resM, resE, armor);
		defsById.emplace_back(this, ud, opts, resM, resE, armor);

		defsByName[ud->GetName()] = &defsById.back();

		const float dcr = ud->GetDecloakDistance();
		if (outDcr < dcr) {
			outDcr = dcr;
		}
	}

	delete resM;
	delete resE;

	for (CCircuitDef& cdef : GetCircuitDefs()) {
		cdef.Init(this);
	}
}

void CCircuitAI::BindUnitToWeaponDefs(CCircuitDef::Id unitDefId, const std::set<CWeaponDef::Id>& weaponDefs, bool isMobile)
{
	if (isMobile) {
		for (CWeaponDef::Id weaponDefId : weaponDefs) {
			SWeaponToUnitDef& wuDef = weaponToUnitDefs[weaponDefId];
			wuDef.mobileIds.insert(unitDefId);
			wuDef.ids.insert(unitDefId);
		}
	} else {
		for (CWeaponDef::Id weaponDefId : weaponDefs) {
			SWeaponToUnitDef& wuDef = weaponToUnitDefs[weaponDefId];
			wuDef.staticIds.insert(unitDefId);
			wuDef.ids.insert(unitDefId);
		}
	}
}

void CCircuitAI::InitWeaponDefs()
{
	Resource* resM = callback->GetResourceByName(RES_NAME_METAL);
	Resource* resE = callback->GetResourceByName(RES_NAME_ENERGY);
	auto weapDefs = callback->GetWeaponDefs();
	weaponDefs.reserve(weapDefs.size());
	for (WeaponDef* wd : weapDefs) {
		// new CWeaponDef(wd, resM, resE);
		weaponDefs.emplace_back(wd, resM, resE);
	}
	delete resM;
	delete resE;
	weaponToUnitDefs.resize(weapDefs.size());
}

CThreatMap* CCircuitAI::GetThreatMap() const
{
	return mapManager->GetThreatMap();
}

CInfluenceMap* CCircuitAI::GetInflMap() const
{
	return mapManager->GetInflMap();
}

int CCircuitAI::GetEnemyTeamSize() const
{
	return callback->GetEnemyTeamSize();
}

void CCircuitAI::CreateGameAttribute()
{
	if (gameAttribute == nullptr) {
		gameAttribute = std::unique_ptr<CGameAttribute>(new CGameAttribute());
		CCircuitDef::InitStatic(this, &gameAttribute->GetRoleMasker(), &gameAttribute->GetAttrMasker());
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

void CCircuitAI::PrepareAreaUpdate()
{
	GetPathfinder()->SetAreaUpdated(false);  // one pathfinder for few allies
	GetEnemyManager()->SetAreaUpdated(false);  // one enemy manager for few allies
}

} // namespace circuit
