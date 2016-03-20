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
#include "Game.h"
#include "Map.h"
#include "Log.h"
#include "Pathing.h"
#include "Drawer.h"
#include "Resource.h"
#include "SkirmishAI.h"
#include "WrappUnit.h"
#include "OptionValues.h"
#include "WrappTeam.h"
#include "Mod.h"
#include "Cheats.h"
//#include "WrappCurrentCommand.h"

namespace circuit {

using namespace springai;

#define RELEASE_NO_CONFIG	100
#ifdef DEBUG
	#define PRINT_TOPIC(txt, topic)	LOG("<CircuitAI> %s topic: %i, SkirmishAIId: %i", txt, topic, skirmishAIId)
#else
	#define PRINT_TOPIC(txt, topic)
#endif

std::unique_ptr<CGameAttribute> CCircuitAI::gameAttribute(nullptr);
unsigned int CCircuitAI::gaCounter = 0;

CCircuitAI::CCircuitAI(OOAICallback* callback)
		: eventHandler(&CCircuitAI::HandleGameEvent)
		, allyTeam(nullptr)
		, difficulty(Difficulty::NORMAL)
		, allyAware(true)
		, initialized(false)
		, lastFrame(0)
		, skirmishAIId(callback != NULL ? callback->GetSkirmishAIId() : -1)
		, sAICallback(nullptr)
		, callback(callback)
		, log(std::unique_ptr<Log>(callback->GetLog()))
		, game(std::unique_ptr<Game>(callback->GetGame()))
		, map(std::unique_ptr<Map>(callback->GetMap()))
		, pathing(std::unique_ptr<Pathing>(callback->GetPathing()))
		, drawer(std::unique_ptr<Drawer>(map->GetDrawer()))
		, skirmishAI(std::unique_ptr<SkirmishAI>(callback->GetSkirmishAI()))
		, airCategory(0)
		, landCategory(0)
		, waterCategory(0)
		, badCategory(0)
		, losResConv(.0f)
#ifdef DEBUG_VIS
		, debugDrawer(nullptr)
#endif
{
	teamId = skirmishAI->GetTeamId();
	team = std::unique_ptr<Team>(WrappTeam::GetInstance(skirmishAIId, teamId));
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
	return (this->*eventHandler)(topic, data);
}

void CCircuitAI::NotifyGameEnd()
{
	eventHandler = &CCircuitAI::HandleEndEvent;
}

int CCircuitAI::HandleGameEvent(int topic, const void* data)
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
			CCircuitUnit* builder = GetTeamUnit(evt->builder);
			CCircuitUnit* unit = RegisterTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitCreated(unit, builder) : ERROR_UNIT_CREATED;
			break;
		}
		case EVENT_UNIT_FINISHED: {
			PRINT_TOPIC("EVENT_UNIT_FINISHED", topic);
			struct SUnitFinishedEvent* evt = (struct SUnitFinishedEvent*)data;
			// Lua might call SetUnitHealth within eventHandler.UnitCreated(this, builder);
			// and trigger UnitFinished before eoh->UnitCreated(*this, builder);
			// @see rts/Sim/Units/Unit.cpp CUnit::PostInit
			CCircuitUnit* unit = RegisterTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitFinished(unit) : ERROR_UNIT_FINISHED;
			break;
		}
		case EVENT_UNIT_IDLE: {
			PRINT_TOPIC("EVENT_UNIT_IDLE", topic);
			struct SUnitIdleEvent* evt = (struct SUnitIdleEvent*)data;
			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitIdle(unit) : ERROR_UNIT_IDLE;
			break;
		}
		case EVENT_UNIT_MOVE_FAILED: {
			PRINT_TOPIC("EVENT_UNIT_MOVE_FAILED", topic);
			struct SUnitMoveFailedEvent* evt = (struct SUnitMoveFailedEvent*)data;
			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitMoveFailed(unit) : ERROR_UNIT_MOVE_FAILED;
			break;
		}
		case EVENT_UNIT_DAMAGED: {
			PRINT_TOPIC("EVENT_UNIT_DAMAGED", topic);
			struct SUnitDamagedEvent* evt = (struct SUnitDamagedEvent*)data;
			CEnemyUnit* attacker = GetEnemyUnit(evt->attacker);
			CCircuitUnit* unit = GetTeamUnit(evt->unit);
			ret = (unit != nullptr) ? this->UnitDamaged(unit, attacker) : ERROR_UNIT_DAMAGED;
			break;
		}
		case EVENT_UNIT_DESTROYED: {
			PRINT_TOPIC("EVENT_UNIT_DESTROYED", topic);
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
			struct SUnitGivenEvent* evt = (struct SUnitGivenEvent*)data;
			ret = this->UnitGiven(evt->unitId, evt->oldTeamId, evt->newTeamId);
			break;
		}
		case EVENT_UNIT_CAPTURED: {
			PRINT_TOPIC("EVENT_UNIT_CAPTURED", topic);
			struct SUnitCapturedEvent* evt = (struct SUnitCapturedEvent*)data;
			ret = this->UnitCaptured(evt->unitId, evt->oldTeamId, evt->newTeamId);
			break;
		}
		case EVENT_ENEMY_ENTER_LOS: {
			PRINT_TOPIC("EVENT_ENEMY_ENTER_LOS", topic);
			struct SEnemyEnterLOSEvent* evt = (struct SEnemyEnterLOSEvent*)data;
			CEnemyUnit* unit = RegisterEnemyUnit(evt->enemy, true);
			ret = (unit != nullptr) ? this->EnemyEnterLOS(unit) : ERROR_ENEMY_ENTER_LOS;
			break;
		}
		case EVENT_ENEMY_LEAVE_LOS: {
			PRINT_TOPIC("EVENT_ENEMY_LEAVE_LOS", topic);
			if (difficulty == Difficulty::HARD) {
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
			struct SEnemyEnterRadarEvent* evt = (struct SEnemyEnterRadarEvent*)data;
			CEnemyUnit* unit = RegisterEnemyUnit(evt->enemy, false);
			ret = (unit != nullptr) ? this->EnemyEnterRadar(unit) : ERROR_ENEMY_ENTER_RADAR;
			break;
		}
		case EVENT_ENEMY_LEAVE_RADAR: {
			PRINT_TOPIC("EVENT_ENEMY_LEAVE_RADAR", topic);
			if (difficulty == Difficulty::HARD) {
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
			struct SEnemyDamagedEvent* evt = (struct SEnemyDamagedEvent*)data;
			CEnemyUnit* enemy = GetEnemyUnit(evt->enemy);
			ret = (enemy != nullptr) ? this->EnemyDamaged(enemy) : ERROR_ENEMY_DAMAGED;
			break;
		}
		case EVENT_ENEMY_DESTROYED: {
			PRINT_TOPIC("EVENT_ENEMY_DESTROYED", topic);
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
			struct SEnemyCreatedEvent* evt = (struct SEnemyCreatedEvent*)data;
			CEnemyUnit* unit = RegisterEnemyUnit(evt->enemy, true);
			ret = (unit != nullptr) ? this->EnemyEnterLOS(unit) : ERROR_ENEMY_ENTER_LOS;
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

int CCircuitAI::HandleEndEvent(int topic, const void* data)
{
	if (topic == EVENT_RELEASE) {
		PRINT_TOPIC("EVENT_RELEASE::END", topic);
		struct SReleaseEvent* evt = (struct SReleaseEvent*)data;
		return this->Release(evt->reason);
	}
	return 0;
}

bool CCircuitAI::IsModValid()
{
	const int minEngineVer = 100;
	const char* engineVersion = sAICallback->Engine_Version_getMajor(skirmishAIId);
	int ver = atoi(engineVersion);
	if (ver < minEngineVer) {
		LOG("Engine must be 100.0 or higher! (%s)", engineVersion);
		return false;
	}

	Mod* mod = callback->GetMod();
	const char* name = mod->GetHumanName();
	const char* version = mod->GetVersion();
	delete mod;
	if ((name == nullptr) || (version == nullptr)) {
		LOG("Can't get name or version of the game. Aborting!");  // NOTE: Sign of messed up spring/AI installation
		return false;
	}

	if ((strstr(name, "Zero-K") == nullptr)) {
		LOG("Only Zero-K game is supported! (%s)", name);
		return false;
	}

	const int minModVer[] = {1, 4, 2, 7};
	unsigned i = 0;
	char* tmp = new char [strlen(version) + 1];
	strcpy(tmp, version);
	const char* tok = strtok(tmp, "v.");
	if (strcmp(tmp, tok) != 0) {  // allow non-standart $VERSION
		while (tok != nullptr) {
			int ver = atoi(tok);
			if (ver < minModVer[i]) {
				delete[] tmp;
				LOG("Zero-K must be 1.4.2.7 or higher! (%s)", version);
				return false;
			}
			if ((ver > minModVer[i]) || (++i >= sizeof(minModVer) / sizeof(minModVer[0]))) {
				break;
			}
			tok = strtok(nullptr, ".");
		}
	}
	delete[] tmp;

	return true;
}

int CCircuitAI::Init(int skirmishAIId, const struct SSkirmishAICallback* sAICallback)
{
	this->skirmishAIId = skirmishAIId;
	// FIXME: Due to chewed API only SSkirmishAICallback have access to Engine
	this->sAICallback = sAICallback;
	if (!IsModValid()) {
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

	std::string cfgName = InitOptions();
	InitUnitDefs();  // Inits TerrainData

	setupManager = std::make_shared<CSetupManager>(this, &gameAttribute->GetSetupData());
	if (!setupManager->OpenConfig(cfgName)) {
		Release(RELEASE_NO_CONFIG);
		return ERROR_INIT;
	}
	allyTeam = setupManager->GetAllyTeam();
	allyAware &= allyTeam->GetSize() > 1;

	allyTeam->Init(this);
	metalManager = allyTeam->GetMetalManager();
	pathfinder = allyTeam->GetPathfinder();

	terrainManager = std::make_shared<CTerrainManager>(this, &gameAttribute->GetTerrainData());

	// NOTE: EconomyManager uses metal clusters and must be initialized after MetalManager::ClusterizeMetal
	economyManager = std::make_shared<CEconomyManager>(this);

	if (setupManager->HasStartBoxes() && setupManager->CanChooseStartPos()) {
		if (metalManager->HasMetalSpots()) {
			setupManager->PickStartPos(this, CSetupManager::StartPosType::METAL_SPOT);
		} else {
			setupManager->PickStartPos(this, CSetupManager::StartPosType::MIDDLE);
		}
	}
	setupManager->PickCommander();

	builderManager = std::make_shared<CBuilderManager>(this);
	factoryManager = std::make_shared<CFactoryManager>(this);
	militaryManager = std::make_shared<CMilitaryManager>(this);

	// TODO: Remove EconomyManager from module (move abilities to BuilderManager).
	modules.push_back(economyManager);
	modules.push_back(militaryManager);
	modules.push_back(builderManager);
	modules.push_back(factoryManager);

	threatMap = std::make_shared<CThreatMap>(this);
	const int offset = skirmishAIId % FRAMES_PER_SEC;
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CCircuitAI::UpdateEnemyUnits, this), FRAMES_PER_SEC, offset);

	if (difficulty == Difficulty::HARD) {
		Cheats* cheats = callback->GetCheats();
		cheats->SetEnabled(true);
		cheats->SetEventsEnabled(true);
		delete cheats;
	}

	setupManager->CloseConfig();

	initialized = true;

	// FIXME: DEBUG
//	if (skirmishAIId == 1) {
//		scheduler->RunTaskAt(std::make_shared<CGameTask>([this]() {
//			terrainManager->ClusterizeTerrain();
//		}));
//	}
	// FIXME: DEBUG

	return 0;  // signaling: OK
}

int CCircuitAI::Release(int reason)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	if (reason == 1) {
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

	for (auto& kv : teamUnits) {
		delete kv.second;
	}
	teamUnits.clear();
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

	initialized = false;

	return 0;  // signaling: OK
}

int CCircuitAI::Update(int frame)
{
	lastFrame = frame;

	scheduler->ProcessTasks(frame);

#ifdef DEBUG_VIS
	if (frame % FRAMES_PER_SEC == 0) {
		allyTeam->GetEnergyLink()->UpdateVis();
		debugDrawer->Refresh();
	}
#endif

	return 0;  // signaling: OK
}

int CCircuitAI::Message(int playerId, const char* message)
{
	const char cmdPos[]    = "~стройсь\0";
	const char cmdSelfD[]  = "~Згинь, нечистая сила!\0";
#ifdef DEBUG_VIS
	const char cmdBlock[]  = "~block\0";
	const char cmdThreat[] = "~threat\0";
	const char cmdArea[]   = "~area\0";
	const char cmdGrid[]   = "~grid\0";
	const char cmdPath[]   = "~path\0";

	const char cmdName[]   = "~name";
	const char cmdEnd[]    = "~end";
#endif

	if (message[0] != '~') {
		return 0;
	}

	size_t msgLength = strlen(message);

	if ((msgLength == strlen(cmdPos)) && (strcmp(message, cmdPos) == 0)) {
		setupManager->PickStartPos(this, CSetupManager::StartPosType::RANDOM);
	}

	else if ((msgLength == strlen(cmdSelfD)) && (strcmp(message, cmdSelfD) == 0)) {
		std::vector<Unit*> units = callback->GetTeamUnits();
		for (auto u : units) {
			u->SelfDestruct();
		}
		utils::free_clear(units);
	}

#ifdef DEBUG_VIS
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
				allyTeam->GetEnergyLink()->ToggleVis();
			}
			utils::free_clear(selection);
		} else if (allyTeam->GetEnergyLink()->IsVis()) {
			allyTeam->GetEnergyLink()->ToggleVis();
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
		unit->GetUnit()->Stop();
		unit->GetUnit()->SetMoveState(2);
		UnitDestroyed(unit, nullptr);
		UnregisterTeamUnit(unit);
	} else if (unit->GetTask() != nullptr) {
		unit->GetTask()->OnUnitMoveFailed(unit);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
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

int CCircuitAI::UnitGiven(CCircuitUnit::Id unitId, int oldTeamId, int newTeamId)
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

	CCircuitUnit* unit = RegisterTeamUnit(unitId);
	if (unit == nullptr) {
		return ERROR_UNIT_GIVEN;
	}

	for (auto& module : modules) {
		module->UnitGiven(unit, oldTeamId, newTeamId);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::UnitCaptured(CCircuitUnit::Id unitId, int oldTeamId, int newTeamId)
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

	threatMap->EnemyEnterLOS(enemy);

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
	threatMap->EnemyDestroyed(enemy);

	return 0;  // signaling: OK
}

int CCircuitAI::PlayerCommand(std::vector<CCircuitUnit*>& units)
{
	for (CCircuitUnit* unit : units) {
		if ((unit != nullptr) && (unit->GetTask() != nullptr)) {
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

int CCircuitAI::LuaMessage(const char* inData)
{
//	if (strncmp(inData, "METAL_SPOTS:", 12) == 0) {
//		gameAttribute->ParseMetalSpots(inData + 12);
//	}
	return 0;  // signaling: OK
}

CCircuitUnit* CCircuitAI::RegisterTeamUnit(CCircuitUnit::Id unitId)
{
	CCircuitUnit* unit = GetTeamUnit(unitId);
	if (unit != nullptr) {
		return unit;
	}

	Unit* u = WrappUnit::GetInstance(skirmishAIId, unitId);
	if (u == nullptr) {
		return nullptr;
	}
	UnitDef* unitDef = u->GetDef();
	CCircuitDef* cdef = GetCircuitDef(unitDef->GetUnitDefId());
	unit = new CCircuitUnit(u, cdef);
	delete unitDef;

	unit->SetArea(terrainManager->GetCurrentMapArea(cdef, unit->GetPos(lastFrame)));

	teamUnits[unitId] = unit;
	cdef->Inc();

	return unit;
}

void CCircuitAI::UnregisterTeamUnit(CCircuitUnit* unit)
{
	teamUnits.erase(unit->GetId());
	defsById[unit->GetCircuitDef()->GetId()]->Dec();

	delete unit;
}

CCircuitUnit* CCircuitAI::GetTeamUnit(CCircuitUnit::Id unitId) const
{
	auto it = teamUnits.find(unitId);
	return (it != teamUnits.end()) ? it->second : nullptr;
}

CCircuitUnit* CCircuitAI::GetFriendlyUnit(Unit* u) const
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

CEnemyUnit* CCircuitAI::RegisterEnemyUnit(CCircuitUnit::Id unitId, bool isInLOS)
{
	CEnemyUnit* unit = GetEnemyUnit(unitId);
	if (unit != nullptr) {
		if (isInLOS/* && (unit->GetCircuitDef() == nullptr)*/) {
			UnitDef* unitDef = unit->GetUnit()->GetDef();
			unit->SetCircuitDef(defsById[unitDef->GetUnitDefId()]);
			delete unitDef;
		}
		return unit;
	}

	springai::Unit* u = WrappUnit::GetInstance(skirmishAIId, unitId);
	if (u == nullptr) {
		return nullptr;
	}
	CCircuitDef* cdef = nullptr;
	if (isInLOS) {
		UnitDef* unitDef = u->GetDef();
		cdef = defsById[unitDef->GetUnitDefId()];
		delete unitDef;
	}
	unit = new CEnemyUnit(u, cdef);

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

		// FIXME: Unit id validation. No EnemyDestroyed sometimes apparently
		if (enemy->IsInRadarOrLOS() && (enemy->GetUnit()->GetPos() == ZeroVector)) {
			EnemyDestroyed(enemy);
			it = enemyUnits.erase(it);  // UnregisterEnemyUnit(enemy)
			delete enemy;
			continue;
		}

		int frame = enemy->GetLastSeen();
		if ((frame != -1) && (lastFrame - frame >= FRAMES_PER_SEC * 600)) {
			EnemyDestroyed(enemy);
			it = enemyUnits.erase(it);  // UnregisterEnemyUnit(enemy)
			delete enemy;
		} else {
			++it;
		}
	}

	threatMap->Update();
}

CEnemyUnit* CCircuitAI::GetEnemyUnit(CCircuitUnit::Id unitId) const
{
	auto it = enemyUnits.find(unitId);
	return (it != enemyUnits.end()) ? it->second : nullptr;
}

std::string CCircuitAI::InitOptions()
{
	OptionValues* options = skirmishAI->GetOptionValues();
	const char* value;
	const char easy[] = "easy";
	const char normal[] = "normal";
	const char hard[] = "hard";
	const char trueVal0[] = "true";
	const char trueVal1[] = "1";

	value = options->GetValueByKey("difficulty");
	if (value != nullptr) {
		if (strncmp(value, easy, sizeof(easy)) == 0) {
			difficulty = Difficulty::EASY;
		} else if (strncmp(value, normal, sizeof(normal)) == 0) {
			difficulty = Difficulty::NORMAL;
		} else if (strncmp(value, hard, sizeof(hard)) == 0) {
			difficulty = Difficulty::HARD;
		}
	}

	value = options->GetValueByKey("ally_aware");
	if (value != nullptr) {
		allyAware = (strncmp(value, trueVal0, sizeof(trueVal0)) == 0) ||
					(strncmp(value, trueVal1, sizeof(trueVal1)) == 0);
	}

	std::string cfgName;
	value = options->GetValueByKey("config");
	if ((value != nullptr) && strlen(value) > 0) {
		cfgName = value;
	} else {
		const char* configs[] = {easy, normal};
		cfgName = configs[std::min(static_cast<size_t>(difficulty), sizeof(configs) / sizeof(configs[0]) - 1)];
	}

	delete options;
	return cfgName;
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

void CCircuitAI::InitUnitDefs()
{
	airCategory   = game->GetCategoriesFlag("FIXEDWING GUNSHIP");
	landCategory  = game->GetCategoriesFlag("LAND SINK TURRET SHIP SWIM FLOAT HOVER");
	waterCategory = game->GetCategoriesFlag("SUB");
	badCategory   = game->GetCategoriesFlag("TERRAFORM STUPIDTARGET MINE");
	Mod* mod = callback->GetMod();
	losResConv = SQUARE_SIZE << mod->GetLosMipLevel();
	delete mod;

	if (!gameAttribute->GetTerrainData().IsInitialized()) {
		gameAttribute->GetTerrainData().Init(this);
	}
	Resource* res = callback->GetResourceByName("Metal");
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
	}
	delete res;

	for (auto& kv : GetCircuitDefs()) {
		kv.second->Init(this);
	}

	// FIXME: DEBUG
//	std::vector<CCircuitDef*> defs;
//	for (auto& kv : GetCircuitDefs()) {
//		if (kv.second->IsMobile()) {
//			defs.push_back(kv.second);
//		}
//	}
//	std::sort(defs.begin(), defs.end(), [](CCircuitDef* a, CCircuitDef* b) {
//		return a->GetUnitDef()->GetLosRadius() * sqrtf(a->GetSpeed()) / a->GetCost() > b->GetUnitDef()->GetLosRadius() * sqrtf(b->GetSpeed()) / b->GetCost();
//	});
//	for (auto d : defs) {
//		LOG("%s\t| %s\t| speed: %f\t| cost: %f\t| dps: %f\t| power: %f", d->GetUnitDef()->GetName(), d->GetUnitDef()->GetHumanName(), d->GetSpeed(), d->GetCost(), d->GetDPS(), d->GetPower());
//	}
	// FIXME: DEBUG
}

//// debug
//void CCircuitAI::DrawClusters()
//{
//	gameAttribute->GetMetalData().DrawConvexHulls(GetDrawer());
//	gameAttribute->GetMetalManager().DrawCentroids(GetDrawer());
//}

void CCircuitAI::CreateGameAttribute()
{
	if (gameAttribute == nullptr) {
		gameAttribute = std::unique_ptr<CGameAttribute>(new CGameAttribute());
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
