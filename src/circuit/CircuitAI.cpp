/*
 * Circuit.cpp
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#include "CircuitAI.h"
#include "static/GameAttribute.h"
#include "static/SetupManager.h"
#include "static/MetalManager.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "module/EconomyManager.h"
#include "module/MilitaryManager.h"
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
#include "terrain/TerrainManager.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AISEvents.h"
#include "AISCommands.h"
#include "SSkirmishAICallback.h"	// "direct" C API
#include "OOAICallback.h"			// C++ wrapper
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
#include "OptionValues.h"

//#include "Command.h"
//#include "WrappCurrentCommand.h"
//#include "Cheats.h"

namespace circuit {

using namespace springai;

#ifdef DEBUG
	#define PRINT_TOPIC(txt, topic)	LOG("<CircuitAI> %s topic: %i, SkirmishAIId: %i", txt, topic, skirmishAIId)
#else
	#define PRINT_TOPIC(txt, topic)
#endif

std::unique_ptr<CGameAttribute> CCircuitAI::gameAttribute(nullptr);
unsigned int CCircuitAI::gaCounter = 0;

CCircuitAI::CCircuitAI(OOAICallback* callback) :
		initialized(false),
		eventHandler(&CCircuitAI::HandleGameEvent),
		lastFrame(-1),
		lastAllyUpdate(-1),
		callback(callback),
		log(std::unique_ptr<Log>(callback->GetLog())),
		game(std::unique_ptr<Game>(callback->GetGame())),
		map(std::unique_ptr<Map>(callback->GetMap())),
		pathing(std::unique_ptr<Pathing>(callback->GetPathing())),
		drawer(std::unique_ptr<Drawer>(map->GetDrawer())),
		skirmishAI(std::unique_ptr<SkirmishAI>(callback->GetSkirmishAI())),
		skirmishAIId(callback != NULL ? callback->GetSkirmishAIId() : -1),
		mexDef(nullptr),
		difficulty(Difficulty::NORMAL),
		allyAware(true)
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
			CCircuitUnit* builder = GetTeamUnitById(evt->builder);
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
			CCircuitUnit* unit = GetTeamUnitById(evt->unit);
			ret = (unit != nullptr) ? this->UnitIdle(unit) : ERROR_UNIT_IDLE;
			break;
		}
		case EVENT_UNIT_MOVE_FAILED: {
			PRINT_TOPIC("EVENT_UNIT_MOVE_FAILED", topic);
			struct SUnitMoveFailedEvent* evt = (struct SUnitMoveFailedEvent*)data;
			CCircuitUnit* unit = GetTeamUnitById(evt->unit);
			ret = (unit != nullptr) ? this->UnitMoveFailed(unit) : ERROR_UNIT_MOVE_FAILED;
			break;
		}
		case EVENT_UNIT_DAMAGED: {
			PRINT_TOPIC("EVENT_UNIT_DAMAGED", topic);
			struct SUnitDamagedEvent* evt = (struct SUnitDamagedEvent*)data;
			CCircuitUnit* attacker = GetEnemyUnitById(evt->attacker);
			CCircuitUnit* unit = GetTeamUnitById(evt->unit);
			ret = (unit != nullptr) ? this->UnitDamaged(unit, attacker) : ERROR_UNIT_DAMAGED;
			break;
		}
		case EVENT_UNIT_DESTROYED: {
			PRINT_TOPIC("EVENT_UNIT_DESTROYED", topic);
			struct SUnitDestroyedEvent* evt = (struct SUnitDestroyedEvent*)data;
			CCircuitUnit* attacker = GetEnemyUnitById(evt->attacker);
			CCircuitUnit* unit = GetTeamUnitById(evt->unit);
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
			CCircuitUnit* unit = RegisterTeamUnit(evt->unitId);
			ret = (unit != nullptr) ? this->UnitGiven(unit, evt->oldTeamId, evt->newTeamId) : ERROR_UNIT_GIVEN;
			break;
		}
		case EVENT_UNIT_CAPTURED: {
			PRINT_TOPIC("EVENT_UNIT_CAPTURED", topic);
			struct SUnitCapturedEvent* evt = (struct SUnitCapturedEvent*)data;
			CCircuitUnit* unit = GetTeamUnitById(evt->unitId);
			if (unit != nullptr) {
				ret = this->UnitCaptured(unit, evt->oldTeamId, evt->newTeamId);
				UnregisterTeamUnit(unit);
			} else {
				ret = ERROR_UNIT_CAPTURED;
			}
			break;
		}
		case EVENT_ENEMY_ENTER_LOS: {
			PRINT_TOPIC("EVENT_ENEMY_ENTER_LOS", topic);
			struct SEnemyEnterLOSEvent* evt = (struct SEnemyEnterLOSEvent*)data;
			CCircuitUnit* unit = RegisterEnemyUnit(evt->enemy);
			ret = (unit != nullptr) ? this->EnemyEnterLOS(unit) : ERROR_ENEMY_ENTER_LOS;
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
			struct SEnemyDestroyedEvent* evt = (struct SEnemyDestroyedEvent*)data;
			CCircuitUnit* enemy = GetEnemyUnitById(evt->enemy);
			if (enemy != nullptr) {
				ret = 0;
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
				units.push_back(GetTeamUnitById(evt->unitIds[i]));
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
//			struct SCommandFinishedEvent* evt = (struct SCommandFinishedEvent*)data;
//			printf("commandId: %i, commandTopicId: %i, unitId: %i\n", evt->commandId, evt->commandTopicId, evt->unitId);
//			CCircuitUnit* unit = GetTeamUnitById(evt->unitId);
//			this->CommandFinished(unit, evt->commandTopicId);

			// FIXME: commandId always == -1
//			springai::Command* command = WrappCurrentCommand::GetInstance(skirmishAIId, evt->unitId, evt->commandId);
//			this->CommandFinished(evt->commandId, evt->commandTopicId, unit);
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

int CCircuitAI::HandleEndEvent(int topic, const void* data)
{
	if (topic == EVENT_RELEASE) {
		PRINT_TOPIC("EVENT_RELEASE::END", topic);
		struct SReleaseEvent* evt = (struct SReleaseEvent*)data;
		return this->Release(evt->reason);
	}
	return 0;
}

int CCircuitAI::Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback)
{
	this->skirmishAIId = skirmishAIId;
	CreateGameAttribute();
	scheduler = std::make_shared<CScheduler>();
	scheduler->Init(scheduler);

	InitOptions();
	InitUnitDefs(callback->GetUnitDefs());

	setupManager = std::make_shared<CSetupManager>(this, &gameAttribute->GetSetupData());
	metalManager = std::make_shared<CMetalManager>(this, &gameAttribute->GetMetalData());
	terrainManager = std::make_shared<CTerrainManager>(this);

	bool canChooseStartPos = (setupManager->HasStartBoxes() && setupManager->CanChooseStartPos());
	if (metalManager->HasMetalSpots()) {
		if (!metalManager->HasMetalClusters() && !metalManager->IsClusterizing()) {
			metalManager->ClusterizeMetal();
		}
		if (canChooseStartPos) {
			// Parallel task is only to ensure its execution after CMetalManager::Clusterize
			scheduler->RunParallelTask(std::make_shared<CGameTask>([this]() {
				setupManager->PickStartPos(this, CSetupManager::StartPosType::METAL_SPOT);
			}));
		}
	} else if (canChooseStartPos) {
		setupManager->PickStartPos(this, CSetupManager::StartPosType::MIDDLE);
	}

	builderManager = std::make_shared<CBuilderManager>(this);
	factoryManager = std::make_shared<CFactoryManager>(this);
	economyManager = std::make_shared<CEconomyManager>(this);
	militaryManager = std::make_shared<CMilitaryManager>(this);

	modules.push_back(economyManager);
	modules.push_back(militaryManager);
	modules.push_back(builderManager);
	modules.push_back(factoryManager);

//	Cheats* cheats = callback->GetCheats();
//	cheats->SetEnabled(true);
//	cheats->SetEventsEnabled(true);
//	delete cheats;

	initialized = true;

	// debug
//	if (skirmishAIId == 1) {
//		scheduler->RunTaskAt(std::make_shared<CGameTask>([this]() {
//			terrainManager->ClusterizeTerrain();
//		}));
//	}

	return 0;  // signaling: OK
}

int CCircuitAI::Release(int reason)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
//	gameAttribute->SetGameEnd(true);

	modules.clear();
	militaryManager = nullptr;
	economyManager = nullptr;
	factoryManager = nullptr;
	builderManager = nullptr;
	terrainManager = nullptr;
	metalManager = nullptr;
	setupManager = nullptr;
	scheduler = nullptr;
	for (auto& kv : teamUnits) {
		delete kv.second;
	}
	teamUnits.clear();
	for (auto& kv : allyUnits) {
		delete kv.second;
	}
	allyUnits.clear();
	for (auto& kv : enemyUnits) {
		delete kv.second;
	}
	enemyUnits.clear();
	for (auto& kv : circuitDefs) {
		delete kv.second;
	}
	circuitDefs.clear();
	for (auto& kv : defsByName) {
		delete kv.second;
	}
	defsByName.clear();
	defsById.clear();
	DestroyGameAttribute();

	initialized = false;

	return 0;  // signaling: OK
}

int CCircuitAI::Update(int frame)
{
	startUpdate = clock::now();
	lastFrame = frame;

	scheduler->ProcessTasks(frame);

	return 0;  // signaling: OK
}

int CCircuitAI::Message(int playerId, const char* message)
{
	const char cmdPos[] = "~стройсь\0";
	const char cmdSelfD[] = "~Згинь, нечистая сила!\0";

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
	Unit* u = unit->GetUnit();
	AIFloat3 pos = u->GetPos();
	AIFloat3 d((float)rand() / RAND_MAX - 0.5f, 0.0f, (float)rand() / RAND_MAX - 0.5f);
	d.Normalize();
	pos += d * SQUARE_SIZE * 10;
	u->MoveTo(pos, 0, FRAMES_PER_SEC * 10);

	return 0;  // signaling: OK
}

int CCircuitAI::UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	for (auto& module : modules) {
		module->UnitDamaged(unit, attacker);
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
	// it might not have been captured from us! Could have been captured from another team
	if (teamId == oldTeamId) {
		for (auto& module : modules) {
			module->UnitCaptured(unit, oldTeamId, newTeamId);
		}
	}

	return 0;  // signaling: OK
}

int CCircuitAI::EnemyEnterLOS(CCircuitUnit* unit)
{
	for (auto& module : modules) {
		module->EnemyEnterLOS(unit);
	}

	return 0;  // signaling: OK
}

int CCircuitAI::PlayerCommand(std::vector<CCircuitUnit*>& units)
{
	for (auto unit : units) {
		// TODO: Replace IdleTask with PlayerTask (commands from player)
		unit->GetTask()->RemoveAssignee(unit);
	}

	return 0;  // signaling: OK
}

//int CCircuitAI::CommandFinished(CCircuitUnit* unit, int commandTopicId)
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

CCircuitUnit* CCircuitAI::RegisterTeamUnit(int unitId)
{
	CCircuitUnit* unit = GetTeamUnitById(unitId);
	if (unit != nullptr) {
		return unit;
	}

	springai::Unit* u = WrappUnit::GetInstance(skirmishAIId, unitId);
	if (u == nullptr) {
		return nullptr;
	}
	UnitDef* unitDef = u->GetDef();
	UnitDef* def = GetUnitDefById(unitDef->GetUnitDefId());
	CCircuitDef* cdef = GetCircuitDef(def);
	unit = new CCircuitUnit(u, def, cdef);
	delete unitDef;

	teamUnits[unitId] = unit;
	cdef->Inc();

	return unit;
}

void CCircuitAI::UnregisterTeamUnit(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	int unitId = u->GetUnitId();

	teamUnits.erase(unitId);
	circuitDefs[unit->GetDef()]->Dec();

	delete unit;
}

CCircuitUnit* CCircuitAI::GetTeamUnitById(int unitId)
{
	std::map<int, CCircuitUnit*>::iterator i = teamUnits.find(unitId);
	if (i != teamUnits.end()) {
		return i->second;
	}

	return nullptr;
}

const std::map<int, CCircuitUnit*>& CCircuitAI::GetTeamUnits() const
{
	return teamUnits;
}

void CCircuitAI::UpdateAllyUnits()
{
	if (lastAllyUpdate >= lastFrame) {
		return;
	}

	for (auto& kv : allyUnits) {
		delete kv.second;
	}
	allyUnits.clear();
	const std::vector<Unit*>& units = callback->GetFriendlyUnits();
	for (auto u : units) {
		// FIXME: Why engine returns vector with some nullptrs?
		// TODO: Check every GetEnemy/FriendlyUnits for nullptr
		if (u == nullptr) {
			continue;
		}
		if (u->GetTeam() != teamId) {
			int unitId = u->GetUnitId();
			UnitDef* unitDef = u->GetDef();
			UnitDef* def = GetUnitDefById(unitDef->GetUnitDefId());
			CCircuitUnit* unit = new CCircuitUnit(u, def, GetCircuitDef(def));
			delete unitDef;
			allyUnits[unitId] = unit;
		}
	}
	lastAllyUpdate = lastFrame;
}

CCircuitUnit* CCircuitAI::GetFriendlyUnit(Unit* u)
{
	if (u->GetTeam() == teamId) {
		return GetTeamUnitById(u->GetUnitId());
	} else if (u->GetAllyTeam() == allyTeamId) {
		return GetAllyUnitById(u->GetUnitId());
	}

	return nullptr;
}

CCircuitUnit* CCircuitAI::GetAllyUnitById(int unitId)
{
	std::map<int, CCircuitUnit*>::iterator i = allyUnits.find(unitId);
	if (i != allyUnits.end()) {
		return i->second;
	}

	return nullptr;
}

const std::map<int, CCircuitUnit*>& CCircuitAI::GetAllyUnits() const
{
	return allyUnits;
}

CCircuitUnit* CCircuitAI::RegisterEnemyUnit(int unitId)
{
	CCircuitUnit* unit = GetEnemyUnitById(unitId);
	if (unit != nullptr) {
		return unit;
	}

	springai::Unit* u = WrappUnit::GetInstance(skirmishAIId, unitId);
	if (u == nullptr) {
		return nullptr;
	}
	UnitDef* unitDef = u->GetDef();
	UnitDef* def = GetUnitDefById(unitDef->GetUnitDefId());
	unit = new CCircuitUnit(u, def, GetCircuitDef(def));
	delete unitDef;

	enemyUnits[unitId] = unit;

	return unit;
}

void CCircuitAI::UnregisterEnemyUnit(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	int unitId = u->GetUnitId();

	enemyUnits.erase(unitId);

	delete unit;
}

CCircuitUnit* CCircuitAI::GetEnemyUnitById(int unitId)
{
	std::map<int, CCircuitUnit*>::iterator i = enemyUnits.find(unitId);
	if (i != enemyUnits.end()) {
		return i->second;
	}

	return nullptr;
}

const std::map<int, CCircuitUnit*>& CCircuitAI::GetEnemyUnits() const
{
	return enemyUnits;
}

bool CCircuitAI::IsUpdateTimeValid()
{
	clock::time_point t = clock::now();
	return std::chrono::duration_cast<milliseconds>(t - startUpdate).count() < 5;  // or (1000 / FRAMES_PER_SEC)
}

void CCircuitAI::InitOptions()
{
	OptionValues* options = skirmishAI->GetOptionValues();
	const char* value;
	const char easy[] = "easy";
	const char normal[] = "normal";
	const char hard[] = "hard";
	const char trueVal[] = "true";

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
		allyAware = (strncmp(value, trueVal, sizeof(trueVal)) == 0);
	}

	delete options;
}

CCircuitAI::Difficulty CCircuitAI::GetDifficulty()
{
	return difficulty;
}

bool CCircuitAI::IsAllyAware()
{
	return allyAware;
}

void CCircuitAI::InitUnitDefs(std::vector<UnitDef*>&& unitDefs)
{
	if (!defsByName.empty()) {
		for (auto& kv : defsByName) {
			delete kv.second;
		}
		defsByName.clear();
		defsById.clear();
		for (auto& kv : circuitDefs) {
			delete kv.second;
		}
	}

	for (auto def : unitDefs) {
		defsByName[def->GetName()] = def;
		defsById[def->GetUnitDefId()] = def;
	}

	for (auto& kv : defsById) {
		std::vector<UnitDef*> options = kv.second->GetBuildOptions();
		std::unordered_set<UnitDef*> opts;
		for (auto buildDef : options) {
			// if it breaks with defsById[] then something really wrong is going on
			opts.insert(defsById[buildDef->GetUnitDefId()]);
		}
		circuitDefs[kv.second] = new CCircuitDef(opts);
		utils::free_clear(options);
	}

	mexDef = GetUnitDefByName("cormex");
}

UnitDef* CCircuitAI::GetUnitDefByName(const char* name)
{
	decltype(defsByName)::iterator i = defsByName.find(name);
	if (i != defsByName.end()) {
		return i->second;
	}

	return nullptr;
}

UnitDef* CCircuitAI::GetUnitDefById(int unitDefId)
{
	decltype(defsById)::iterator i = defsById.find(unitDefId);
	if (i != defsById.end()) {
		return i->second;
	}

	return nullptr;
}

CCircuitAI::UnitDefs& CCircuitAI::GetUnitDefs()
{
	return defsByName;
}

CCircuitDef* CCircuitAI::GetCircuitDef(springai::UnitDef* unitDef)
{
	decltype(circuitDefs)::iterator i = circuitDefs.find(unitDef);
	if (i != circuitDefs.end()) {
		return i->second;
	}

	return nullptr;
}

int CCircuitAI::GetUnitCount(UnitDef* unitDef)
{
	decltype(circuitDefs)::iterator i = circuitDefs.find(unitDef);
	if (i != circuitDefs.end()) {
		return i->second->GetCount();
	}

	return -1;
}

bool CCircuitAI::IsAvailable(UnitDef* unitDef)
{
	return (unitDef->GetMaxThisUnit() > GetUnitCount(unitDef));
}

UnitDef* CCircuitAI::GetMexDef() const
{
	return mexDef;
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

CSetupManager* CCircuitAI::GetSetupManager()
{
	return setupManager.get();
}

CMetalManager* CCircuitAI::GetMetalManager()
{
	return metalManager.get();
}

CTerrainManager* CCircuitAI::GetTerrainManager()
{
	return terrainManager.get();
}

CBuilderManager* CCircuitAI::GetBuilderManager()
{
	return builderManager.get();
}

CFactoryManager* CCircuitAI::GetFactoryManager()
{
	return factoryManager.get();
}

CEconomyManager* CCircuitAI::GetEconomyManager()
{
	return economyManager.get();
}

CMilitaryManager* CCircuitAI::GetMilitaryManager()
{
	return militaryManager.get();
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
			gameAttribute = nullptr;
			// deletes singleton here;
		}
		gaCounter = 0;
	} else {
		gaCounter--;
	}
}

} // namespace circuit
