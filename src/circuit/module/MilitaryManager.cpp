/*
 * MilitaryManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "module/MilitaryManager.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "map/InfluenceMap.h"
#include "map/ThreatMap.h"
#include "resource/MetalManager.h"
#include "setup/SetupManager.h"
#include "setup/DefenceMatrix.h"
#include "task/NilTask.h"
#include "task/IdleTask.h"
#include "task/RetreatTask.h"
#include "task/builder/DefenceTask.h"
#include "task/fighter/RallyTask.h"
#include "task/fighter/GuardTask.h"
#include "task/fighter/DefendTask.h"
#include "task/fighter/ScoutTask.h"
#include "task/fighter/RaidTask.h"
#include "task/fighter/AttackTask.h"
#include "task/fighter/BombTask.h"
#include "task/fighter/ArtilleryTask.h"
#include "task/fighter/AntiAirTask.h"
#include "task/fighter/AntiHeavyTask.h"
#include "task/fighter/SupportTask.h"
#include "task/static/SuperTask.h"
#include "terrain/TerrainManager.h"
#include "terrain/path/PathFinder.h"
#include "terrain/path/QueryPathMulti.h"
#include "unit/enemy/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/Utils.h"
#include "json/json.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "AISCommands.h"
#include "Command.h"
#include "Log.h"

namespace circuit {

using namespace springai;

CMilitaryManager::CMilitaryManager(CCircuitAI* circuit)
		: IUnitModule(circuit)
		, fightIterator(0)
		, defenceIdx(0)
		, scoutIdx(0)
		, armyCost(0.f)
		, radarDef(nullptr)
		, sonarDef(nullptr)
		, bigGunDef(nullptr)
{
	circuit->GetScheduler()->RunOnInit(std::make_shared<CGameTask>(&CMilitaryManager::Init, this));

	/*
	 * Defence handlers
	 */
	auto defenceFinishedHandler = [this](CCircuitUnit* unit) {
		TRY_UNIT(this->circuit, unit,
			unit->GetUnit()->Stockpile(UNIT_COMMAND_OPTION_SHIFT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
		)
	};
	auto defenceDestroyedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
		int frame = this->circuit->GetLastFrame();
		float defCost = unit->GetCircuitDef()->GetCostM();
		CDefenceMatrix::SDefPoint* point = defence->GetDefPoint(unit->GetPos(frame), defCost);
		if (point != nullptr) {
			point->cost -= defCost;
		}
	};

	/*
	 * Attacker handlers
	 */
	auto attackerCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nilTask->AssignTo(unit);
			this->circuit->AddActionUnit(unit);
		}
	};
	auto attackerFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			this->circuit->AddActionUnit(unit);
		}
		idleTask->AssignTo(unit);

		army.insert(unit);
		AddArmyCost(unit);

		TRY_UNIT(this->circuit, unit,
			if (unit->GetCircuitDef()->IsAbleToFly()) {
				if (unit->GetCircuitDef()->IsAttrNoStrafe()) {
					unit->CmdAirStrafe(0);
				}
				if (unit->GetCircuitDef()->IsRoleMine()) {
					unit->GetUnit()->SetIdleMode(1);
				}
			}
			if (unit->GetCircuitDef()->IsAttrStock()) {
				unit->GetUnit()->Stockpile(UNIT_COMMAND_OPTION_SHIFT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
				unit->CmdMiscPriority(2);
			}
		)
	};
	auto attackerIdleHandler = [this](CCircuitUnit* unit) {
		// NOTE: Avoid instant task reassignment, though it may be not relevant for attackers
		if (this->circuit->GetLastFrame() > unit->GetTaskFrame()/* + FRAMES_PER_SEC*/) {
			unit->GetTask()->OnUnitIdle(unit);
		}
	};
	auto attackerDamagedHandler = [](CCircuitUnit* unit, CEnemyInfo* attacker) {
		unit->GetTask()->OnUnitDamaged(unit, attacker);
	};
	auto attackerDestroyedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		if (task->GetType() == IUnitTask::Type::NIL) {
			return;
		}

		DelArmyCost(unit);
		army.erase(unit);
	};

	/*
	 * Superweapon handlers
	 */
	auto superCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nilTask->AssignTo(unit);
			this->circuit->AddActionUnit(unit);
		}
	};
	auto superFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			this->circuit->AddActionUnit(unit);
		}
		idleTask->AssignTo(unit);

		TRY_UNIT(this->circuit, unit,
			unit->GetUnit()->SetTrajectory(1);
			if (unit->GetCircuitDef()->IsAttrStock()) {
				unit->GetUnit()->Stockpile(UNIT_COMMAND_OPTION_SHIFT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
				unit->CmdMiscPriority(2);
			}
		)
	};
	auto superDestroyedHandler = [](CCircuitUnit* unit, CEnemyInfo* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask
	};

	/*
	 * Defend buildings handler
	 */
//	auto structDamagedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
//		const std::set<IFighterTask*>& tasks = GetTasks(IFighterTask::FightType::DEFEND);
//		if (tasks.empty()) {
//			return;
//		}
//		int frame = this->circuit->GetLastFrame();
//		const AIFloat3& pos = unit->GetPos(frame);
//		CTerrainManager* terrainMgr = this->circuit->GetTerrainManager();
//		CDefendTask* defendTask = nullptr;
//		float minSqDist = std::numeric_limits<float>::max();
//		for (IFighterTask* task : tasks) {
//			CDefendTask* dt = static_cast<CDefendTask*>(task);
//			const float sqDist = pos.SqDistance2D(dt->GetPosition());
//			if ((minSqDist <= sqDist) || !terrainMgr->CanMoveToPos(dt->GetLeader()->GetArea(), pos)) {
//				continue;
//			}
//			if ((dt->GetTarget() == nullptr) ||
//				(dt->GetTarget()->GetPos().SqDistance2D(dt->GetLeader()->GetPos(frame)) > sqDist))
//			{
//				minSqDist = sqDist;
//				defendTask = dt;
//			}
//		}
//		if (defendTask != nullptr) {
//			defendTask->SetPosition(pos);
//			defendTask->SetWantedTarget(attacker);
//		}
//	};

	// NOTE: IsRole used below
	ReadConfig();

	for (const CCircuitDef* cdef : defenderDefs) {
		destroyedHandler[cdef->GetId()] = defenceDestroyedHandler;
	}

	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const Json::Value& retreat = root["retreat"]["fighter"];
	const float fighterRet = retreat.get((unsigned)0, 0.5f).asFloat();
	const float retMod = retreat.get((unsigned)1, 1.0f).asFloat();
	const float commMod = root["quota"]["thr_mod"].get("comm", 1.f).asFloat();
	float maxRadarDivCost = 0.f;
	float maxSonarDivCost = 0.f;
	CCircuitDef* commDef = circuit->GetSetupManager()->GetCommChoice();

	for (CCircuitDef& cdef : circuit->GetCircuitDefs()) {
		CCircuitDef::Id unitDefId = cdef.GetId();
		if (cdef.IsRoleComm()) {
			cdef.ModThreat(commMod);
		}
		if (cdef.GetDef()->IsBuilder()) {
//			damagedHandler[unitDefId] = structDamagedHandler;
			continue;
		}
		const std::map<std::string, std::string>& customParams = cdef.GetDef()->GetCustomParams();
		auto it = customParams.find("is_drone");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			continue;
		}
		if (cdef.IsMobile()) {
			createdHandler[unitDefId] = attackerCreatedHandler;
			finishedHandler[unitDefId] = attackerFinishedHandler;
			idleHandler[unitDefId] = attackerIdleHandler;
			damagedHandler[unitDefId] = attackerDamagedHandler;
			destroyedHandler[unitDefId] = attackerDestroyedHandler;

			if (cdef.GetRetreat() < 0.f) {
				cdef.SetRetreat(fighterRet);
			} else {
				cdef.SetRetreat(cdef.GetRetreat() * retMod);
			}
		} else {
//			damagedHandler[unitDefId] = structDamagedHandler;
			if (cdef.IsAttacker()) {
				if (cdef.IsRoleSuper()) {
					createdHandler[unitDefId] = superCreatedHandler;
					finishedHandler[unitDefId] = superFinishedHandler;
					destroyedHandler[unitDefId] = superDestroyedHandler;
				} else if (cdef.IsAttrStock()) {
					finishedHandler[unitDefId] = defenceFinishedHandler;
				}
			}
			if (commDef->CanBuild(&cdef)) {
				float range = cdef.GetDef()->GetRadarRadius();
				float areaDivCost = M_PI * SQUARE(range) / cdef.GetCostM();
				if (maxRadarDivCost < areaDivCost) {
					maxRadarDivCost = areaDivCost;
					radarDef = &cdef;
				}
				range = cdef.GetDef()->GetSonarRadius();
				areaDivCost = M_PI * SQUARE(range) / cdef.GetCostM();
				if (maxSonarDivCost < areaDivCost) {
					maxSonarDivCost = areaDivCost;
					sonarDef = &cdef;
				}
			}
		}
	}

	defence = circuit->GetAllyTeam()->GetDefenceMatrix().get();

	fightTasks.resize(static_cast<IFighterTask::FT>(IFighterTask::FightType::_SIZE_));
}

CMilitaryManager::~CMilitaryManager()
{
	for (IUnitTask* task : fightUpdates) {
		task->ClearRelease();
	}
}

void CMilitaryManager::ReadConfig()
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();
	CCircuitDef::RoleName& roleNames = CCircuitDef::GetRoleNames();

	const Json::Value& responses = root["response"];
	const float teamSize = circuit->GetAllyTeam()->GetSize();
	roleInfos.resize(roleNames.size(), {.0f});
	for (const auto& pair : roleNames) {
		SRoleInfo& info = roleInfos[pair.second.type];
		const Json::Value& response = responses[pair.first];

		if (response.isNull()) {
			info.maxPerc = 1.0f;
			info.factor  = teamSize;
			continue;
		}

		info.maxPerc = response.get("max_percent", 1.0f).asFloat();
		const float step = response.get("eps_step", 1.0f).asFloat();
		info.factor  = (teamSize - 1.0f) * step + 1.0f;

		const Json::Value& vs = response["vs"];
		const Json::Value& ratio = response["ratio"];
		const Json::Value& importance = response["importance"];
		for (unsigned i = 0; i < vs.size(); ++i) {
			const std::string& roleName = vs[i].asString();
			auto it = roleNames.find(roleName);
			if (it == roleNames.end()) {
				circuit->LOG("CONFIG %s: response %s vs unknown role '%s'", cfgName.c_str(), pair.first.c_str(), roleName.c_str());
				continue;
			}
			float rat = ratio.get(i, 1.0f).asFloat();
			float imp = importance.get(i, 1.0f).asFloat();
			info.vs.push_back(SRoleInfo::SVsInfo(it->second.type, rat, imp));
		}
	}

	const Json::Value& quotas = root["quota"];
	maxScouts = quotas.get("scout", 3).asUInt();
	const Json::Value& qraid = quotas["raid"];
	raid.min = qraid.get((unsigned)0, 3.f).asFloat();
	raid.avg = qraid.get((unsigned)1, 5.f).asFloat();
	minAttackers = quotas.get("attack", 8.f).asFloat();
	const Json::Value& qthrMod = quotas["thr_mod"];
	const Json::Value& qthrAtk = qthrMod["attack"];
	attackMod.min = qthrAtk.get((unsigned)0, 1.f).asFloat();
	attackMod.len = qthrAtk.get((unsigned)1, 1.f).asFloat() - attackMod.min;
	const Json::Value& qthrDef = qthrMod["defence"];
	defenceMod.min = qthrDef.get((unsigned)0, 1.f).asFloat();
	defenceMod.len = qthrDef.get((unsigned)1, 1.f).asFloat() - defenceMod.min;

	const Json::Value& porc = root["porcupine"];
	const Json::Value& defs = porc["unit"];
	defenderDefs.reserve(defs.size());
	for (const Json::Value& def : defs) {
		CCircuitDef* cdef = circuit->GetCircuitDef(def.asCString());
		if (cdef == nullptr) {
			circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), def.asCString());
		} else {
			defenderDefs.push_back(cdef);
		}
	}
	const Json::Value& land = porc["land"];
	landDefenders.reserve(land.size());
	for (const Json::Value& idx : land) {
		unsigned index = idx.asUInt();
		if (index < defenderDefs.size()) {
			landDefenders.push_back(defenderDefs[index]);
		}
	}
	const Json::Value& watr = porc["water"];
	waterDefenders.reserve(watr.size());
	for (const Json::Value& idx : watr) {
		unsigned index = idx.asUInt();
		if (index < defenderDefs.size()) {
			waterDefenders.push_back(defenderDefs[index]);
		}
	}

	preventCount = porc.get("prevent", 1).asUInt();
	const Json::Value& amount = porc["amount"];
	const Json::Value& amOff = amount["offset"];
	const Json::Value& amFac = amount["factor"];
	const Json::Value& amMap = amount["map"];
	const float minOffset = amOff.get((unsigned)0, -0.2f).asFloat();
	const float maxOffset = amOff.get((unsigned)1, 0.2f).asFloat();
	const float offset = (float)rand() / RAND_MAX * (maxOffset - minOffset) + minOffset;
	const float minFactor = amFac.get((unsigned)0, 2.0f).asFloat();
	const float maxFactor = amFac.get((unsigned)1, 1.0f).asFloat();
	const float minMap = amMap.get((unsigned)0, 8.0f).asFloat();
	const float maxMap = amMap.get((unsigned)1, 24.0f).asFloat();
	const float mapSize = (circuit->GetMap()->GetWidth() / 64) * (circuit->GetMap()->GetHeight() / 64);
	amountFactor = (maxFactor - minFactor) / (SQUARE(maxMap) - SQUARE(minMap)) * (mapSize - SQUARE(minMap)) + minFactor + offset;
//	amountFactor = std::max(amountFactor, 0.f);

	const Json::Value& base = porc["base"];
	baseDefence.reserve(base.size());
	for (const Json::Value& pair : base) {
		unsigned index = pair.get((unsigned)0, -1).asUInt();
		if (index >= defenderDefs.size()) {
			continue;
		}
		int frame = pair.get((unsigned)1, 0).asInt() * FRAMES_PER_SEC;
		baseDefence.push_back(std::make_pair(defenderDefs[index], frame));
	}
	auto compare = [](const std::pair<CCircuitDef*, int>& d1, const std::pair<CCircuitDef*, int>& d2) {
		return d1.second > d2.second;
	};
	std::sort(baseDefence.begin(), baseDefence.end(), compare);

	const Json::Value& super = porc["superweapon"];
	const Json::Value& items = super["unit"];
	const Json::Value& probs = super["weight"];
	superInfos.reserve(items.size());
	for (unsigned i = 0; i < items.size(); ++i) {
		SSuperInfo si;
		si.cdef = circuit->GetCircuitDef(items[i].asCString());
		if (si.cdef == nullptr) {
			circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), items[i].asCString());
			continue;
		}
		si.cdef->SetMainRole(ROLE_TYPE(SUPER));  // override mainRole
		si.cdef->AddEnemyRole(ROLE_TYPE(SUPER));
		si.cdef->AddRole(ROLE_TYPE(SUPER));
		si.weight = probs.get(i, 1.f).asFloat();
		superInfos.push_back(si);
	}
	DiceBigGun();

	defaultPorc = circuit->GetCircuitDef(porc.get("default", "").asCString());
	if (defaultPorc == nullptr) {
		defaultPorc = circuit->GetEconomyManager()->GetDefaultDef();
	}
}

void CMilitaryManager::Init()
{
	CMetalManager* metalMgr = circuit->GetMetalManager();
	const CMetalData::Metals& spots = metalMgr->GetSpots();
	const CMetalData::Clusters& clusters = metalMgr->GetClusters();

	scoutPath.reserve(spots.size());
	for (unsigned i = 0; i < spots.size(); ++i) {
		scoutPath.push_back(i);
	}

	raidPath.reserve(clusters.size());
	for (unsigned i = 0; i < clusters.size(); ++i) {
		raidPath.push_back({i, -1, 1.f});
	}

	CSetupManager::StartFunc subinit = [this, &spots](const AIFloat3& pos) {
		auto compare = [&pos, &spots](int a, int b) {
			return pos.SqDistance2D(spots[a].position) > pos.SqDistance2D(spots[b].position);
		};
		std::sort(scoutPath.begin(), scoutPath.end(), compare);

		CScheduler* scheduler = circuit->GetScheduler().get();
		const int interval = 4;
		const int offset = circuit->GetSkirmishAIId() % interval;
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::UpdateIdle, this), interval, offset + 0);
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::UpdateFight, this), 1/*interval / 2*/, offset + 1);
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::UpdateDefenceTasks, this), FRAMES_PER_SEC * 5, offset + 2);

		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::Watchdog, this),
								FRAMES_PER_SEC * 60,
								circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 12);
	};

	circuit->GetSetupManager()->ExecOnFindStart(subinit);
}

void CMilitaryManager::Release()
{
	// NOTE: Release expected to be called on CCircuit::Release.
	//       It doesn't stop scheduled GameTasks for that reason.
	for (IUnitTask* task : fightUpdates) {
		AbortTask(task);
		// NOTE: Do not delete task as other AbortTask may ask for it
	}
	for (IUnitTask* task : fightUpdates) {
		task->ClearRelease();
	}
	fightUpdates.clear();
}

int CMilitaryManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetCircuitDef()->GetId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitFinished(CCircuitUnit* unit)
{
	auto search = finishedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetCircuitDef()->GetId());
	if (search != idleHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	auto search = damagedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != damagedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

IFighterTask* CMilitaryManager::EnqueueTask(IFighterTask::FightType type)
{
	IFighterTask* task;
	switch (type) {
		default:
		case IFighterTask::FightType::RALLY: {
//			CEconomyManager* economyMgr = circuit->GetEconomyManager();
//			float power = economyMgr->GetAvgMetalIncome() * economyMgr->GetEcoFactor() * 32.0f;
			task = new CRallyTask(this, /*power*/1);  // TODO: pass enemy's threat
			break;
		}
		case IFighterTask::FightType::SCOUT: {
			const float mod = (float)rand() / RAND_MAX * attackMod.len + attackMod.min;
			task = new CScoutTask(this, 0.75f / mod);
			break;
		}
		case IFighterTask::FightType::RAID: {
			const float mod = (float)rand() / RAND_MAX * attackMod.len + attackMod.min;
			task = new CRaidTask(this, raid.avg, 0.75f / mod);
			break;
		}
		case IFighterTask::FightType::ATTACK: {
			const float mod = (float)rand() / RAND_MAX * attackMod.len + attackMod.min;
			task = new CAttackTask(this, minAttackers, 0.8f / mod);
			break;
		}
		case IFighterTask::FightType::BOMB: {
			const float mod = (float)rand() / RAND_MAX * attackMod.len + attackMod.min;
			task = new CBombTask(this, 2.0f / mod);
			break;
		}
		case IFighterTask::FightType::ARTY: {
			task = new CArtilleryTask(this);
			break;
		}
		case IFighterTask::FightType::AA: {
			const float mod = (float)rand() / RAND_MAX * attackMod.len + attackMod.min;
			task = new CAntiAirTask(this, 1.0f / mod);
			break;
		}
		case IFighterTask::FightType::AH: {
			const float mod = (float)rand() / RAND_MAX * attackMod.len + attackMod.min;
			task = new CAntiHeavyTask(this, 2.0f / mod);
			break;
		}
		case IFighterTask::FightType::SUPPORT: {
			task = new CSupportTask(this);
			break;
		}
		case IFighterTask::FightType::SUPER: {
			task = new CSuperTask(this);
			break;
		}
	}

	fightTasks[static_cast<IFighterTask::FT>(type)].insert(task);
	fightUpdates.push_back(task);
	return task;
}

IFighterTask* CMilitaryManager::EnqueueDefend(IFighterTask::FightType promote, float power)
{
	const float mod = (float)rand() / RAND_MAX * defenceMod.len + defenceMod.min;
	IFighterTask* task = new CDefendTask(this, circuit->GetSetupManager()->GetBasePos(),
										 promote, promote, power, 1.0f / mod);
	fightTasks[static_cast<IFighterTask::FT>(IFighterTask::FightType::DEFEND)].insert(task);
	fightUpdates.push_back(task);
	return task;
}

IFighterTask* CMilitaryManager::EnqueueDefend(IFighterTask::FightType check, IFighterTask::FightType promote, float power)
{
	IFighterTask* task = new CDefendTask(this, circuit->GetSetupManager()->GetBasePos(),
										 check, promote, power, 1.0f);
	fightTasks[static_cast<IFighterTask::FT>(IFighterTask::FightType::DEFEND)].insert(task);
	fightUpdates.push_back(task);
	return task;
}

IFighterTask* CMilitaryManager::EnqueueGuard(CCircuitUnit* vip)
{
	IFighterTask* task = new CFGuardTask(this, vip, 1.0f);
	fightTasks[static_cast<IFighterTask::FT>(IFighterTask::FightType::GUARD)].insert(task);
	fightUpdates.push_back(task);
	return task;
}

CRetreatTask* CMilitaryManager::EnqueueRetreat()
{
	CRetreatTask* task = new CRetreatTask(this);
	fightUpdates.push_back(task);
	return task;
}

void CMilitaryManager::DequeueTask(IUnitTask* task, bool done)
{
	switch (task->GetType()) {
		case IUnitTask::Type::FIGHTER: {
			IFighterTask* taskF = static_cast<IFighterTask*>(task);
			fightTasks[static_cast<IFighterTask::FT>(taskF->GetFightType())].erase(taskF);
		} break;
		default: break;
	}
	task->Dead();
	task->Stop(done);
}

IUnitTask* CMilitaryManager::MakeTask(CCircuitUnit* unit)
{
	return DefaultMakeTask(unit);
}

void CMilitaryManager::AbortTask(IUnitTask* task)
{
	DequeueTask(task, false);
}

void CMilitaryManager::DoneTask(IUnitTask* task)
{
	DequeueTask(task, true);
}

void CMilitaryManager::FallbackTask(CCircuitUnit* unit)
{
}

void CMilitaryManager::MakeDefence(const AIFloat3& pos)
{
	int index = circuit->GetMetalManager()->FindNearestCluster(pos);
	if (index >= 0) {
		MakeDefence(index, pos);
	}
}

void CMilitaryManager::MakeDefence(int cluster)
{
	MakeDefence(cluster, circuit->GetMetalManager()->GetClusters()[cluster].position);
}

void CMilitaryManager::MakeDefence(int cluster, const AIFloat3& pos)
{
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	if (terrainMgr->IsZoneAlly(pos)) {
		return;
	}

	CMetalManager* mm = circuit->GetMetalManager();
	CEconomyManager* em = circuit->GetEconomyManager();
	const float metalIncome = std::min(em->GetAvgMetalIncome(), em->GetAvgEnergyIncome()) * em->GetEcoFactor();
	float maxCost = MIN_BUILD_SEC * amountFactor * metalIncome;
	CDefenceMatrix::SDefPoint* closestPoint = nullptr;
	float minDist = std::numeric_limits<float>::max();
	std::vector<CDefenceMatrix::SDefPoint>& points = defence->GetDefPoints(cluster);
	for (CDefenceMatrix::SDefPoint& defPoint : points) {
		if (defPoint.cost < maxCost) {
			float dist = defPoint.position.SqDistance2D(pos);
			if ((closestPoint == nullptr) || (dist < minDist)) {
				closestPoint = &defPoint;
				minDist = dist;
			}
		}
	}
	if (closestPoint == nullptr) {
		return;
	}
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	float totalCost = .0f;
	IBuilderTask* parentTask = nullptr;
	// NOTE: circuit->GetTerrainManager()->IsWaterSector(pos) checks whole sector
	//       but water recognized as height < 0
	bool isWater = circuit->GetMap()->GetElevationAt(pos.x, pos.z) < -SQUARE_SIZE * 5;
	std::vector<CCircuitDef*>& defenders = isWater ? waterDefenders : landDefenders;

	// Front-line porc
	bool isPorc = mm->GetMaxIncome() > mm->GetAvgIncome() + 1.f;
	if (isPorc) {
		const float income = (mm->GetAvgIncome() + mm->GetMaxIncome()) * 0.5f;
		int spotId = mm->FindNearestSpot(pos);
		isPorc = mm->GetSpots()[spotId].income > income;
	}
	if (!isPorc) {
		unsigned threatCount = 0;
		CThreatMap* threatMap = circuit->GetThreatMap();
		const CMetalData::Clusters& clusters = mm->GetClusters();
		const CMetalData::Metals& spots = mm->GetSpots();
		const CMetalData::ClusterGraph& clusterGraph = mm->GetClusterGraph();
		CMetalData::ClusterGraph::Node node = clusterGraph.nodeFromId(cluster);
		CMetalData::ClusterGraph::IncEdgeIt edgeIt(clusterGraph, node);
		for (; edgeIt != lemon::INVALID; ++edgeIt) {
			int idx0 = clusterGraph.id(clusterGraph.oppositeNode(node, edgeIt));
			if (mm->IsClusterFinished(idx0)) {
				continue;
			}
			// check if there is enemy neighbor
			for (int idx : clusters[idx0].idxSpots) {
				if (threatMap->GetAllThreatAt(spots[idx].position) > THREAT_MIN * 2) {
					threatCount++;
					break;
				}
			}
			if (threatCount >= 2) {  // if 2 nearby clusters are a threat
				isPorc = true;
				break;
			}
		}
	}
	if (!isPorc) {
		for (IBuilderTask* t : builderMgr->GetTasks(IBuilderTask::BuildType::DEFENCE)) {
			if ((t->GetTarget() == nullptr) && (t->GetNextTask() != nullptr) &&
				(closestPoint->position.SqDistance2D(t->GetTaskPos()) < SQUARE(SQUARE_SIZE)))
			{
				builderMgr->AbortTask(t);
				break;
			}
		}
	}
	unsigned num = std::min<unsigned>(isPorc ? defenders.size() : preventCount, defenders.size());

	AIFloat3 backDir = circuit->GetSetupManager()->GetBasePos() - closestPoint->position;
	AIFloat3 backPos = closestPoint->position + backDir.Normalize2D() * SQUARE_SIZE * 16;
	CTerrainManager::CorrectPosition(backPos);

	CEnemyManager* enemyMgr = circuit->GetEnemyManager();
	const int frame = circuit->GetLastFrame();
	for (unsigned i = 0; i < num; ++i) {
		CCircuitDef* defDef = defenders[i];
		if (!defDef->IsAvailable(frame) || (defDef->IsRoleAA() && (enemyMgr->GetEnemyCost(ROLE_TYPE(AIR)) < 1.f))) {
			continue;
		}
		float defCost = defDef->GetCostM();
		totalCost += defCost;
		if (totalCost <= closestPoint->cost) {
			continue;
		}
		if (totalCost < maxCost) {
			closestPoint->cost += defCost;
			bool isFirst = (parentTask == nullptr);
			const AIFloat3& buildPos = defDef->IsAttacker() ? closestPoint->position : backPos;
			IBuilderTask* task = builderMgr->EnqueueTask(IBuilderTask::Priority::HIGH, defDef, buildPos,
					IBuilderTask::BuildType::DEFENCE, defCost, SQUARE_SIZE * 32, isFirst);
			if (parentTask != nullptr) {
				parentTask->SetNextTask(task);
			}
			parentTask = task;
		} else {
			// TODO: Auto-sort defenders by cost OR remove break?
			break;
		}
	}

	// Build sensors
	auto checkSensor = [this, &backPos, builderMgr](IBuilderTask::BuildType type, CCircuitDef* cdef, float range) {
		bool isBuilt = false;
		COOAICallback* clb = circuit->GetCallback();
		auto friendlies = clb->GetFriendlyUnitIdsIn(backPos, range);
		for (int auId : friendlies) {
			if (auId == -1) {
				continue;
			}
			CCircuitDef::Id defId = clb->Unit_GetDefId(auId);
			if (defId == cdef->GetId()) {
				isBuilt = true;
				break;
			}
		}
		if (!isBuilt) {
			const IBuilderTask* task = nullptr;
			const float qdist = SQUARE(range);
			for (const IBuilderTask* t : builderMgr->GetTasks(type)) {
				if (backPos.SqDistance2D(t->GetTaskPos()) < qdist) {
					task = t;
					break;
				}
			}
			if (task == nullptr) {
				builderMgr->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, backPos, type);
			}
		}
	};
	// radar
	if ((radarDef != nullptr) && radarDef->IsAvailable(frame) && (radarDef->GetCostM() < maxCost)) {
		const float range = radarDef->GetDef()->GetRadarRadius() / (isPorc ? 4.f : SQRT_2);
		checkSensor(IBuilderTask::BuildType::RADAR, radarDef, range);
	}
	// sonar
	if (isWater && (sonarDef != nullptr) && sonarDef->IsAvailable(frame) && (sonarDef->GetCostM() < maxCost)) {
		checkSensor(IBuilderTask::BuildType::SONAR, sonarDef, sonarDef->GetDef()->GetSonarRadius());
	}
}

void CMilitaryManager::AbortDefence(const CBDefenceTask* task)
{
	float defCost = task->GetBuildDef()->GetCostM();
	CDefenceMatrix::SDefPoint* point = defence->GetDefPoint(task->GetPosition(), defCost);
	if (point != nullptr) {
		if ((task->GetTarget() == nullptr) && (point->cost >= defCost)) {
			point->cost -= defCost;
		}
		IBuilderTask* next = task->GetNextTask();
		while (next != nullptr) {
			if (next->GetBuildDef() != nullptr) {
				defCost = next->GetBuildDef()->GetCostM();
			} else{
				defCost = next->GetCost();
			}
			if (point->cost >= defCost) {
				point->cost -= defCost;
			}
			next = next->GetNextTask();
		}
	}
}

bool CMilitaryManager::HasDefence(int cluster)
{
	const std::vector<CDefenceMatrix::SDefPoint>& points = defence->GetDefPoints(cluster);
	for (const CDefenceMatrix::SDefPoint& defPoint : points) {
		if (defPoint.cost > .5f) {
			return true;
		}
	}
	return false;
}

AIFloat3 CMilitaryManager::GetScoutPosition(CCircuitUnit* unit)
{
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CMetalManager* metalMgr = circuit->GetMetalManager();
	STerrainMapArea* area = unit->GetArea();
//	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
//	const float minSqRange = SQUARE(unit->GetCircuitDef()->GetLosRadius());
	const CMetalData::Metals& spots = metalMgr->GetSpots();
	decltype(scoutIdx) prevIdx = scoutIdx;
	while (scoutIdx < scoutPath.size()) {
		int index = scoutPath[scoutIdx++];
		if (!metalMgr->IsMexInFinished(index)
			&& terrainMgr->CanMoveToPos(area, spots[index].position)
			/*&& (pos.SqDistance2D(spots[index].position) > minSqRange)*/)
		{
			return spots[index].position;
		}
	}
	scoutIdx = 0;
	while (scoutIdx < prevIdx) {
		int index = scoutPath[scoutIdx++];
		if (!metalMgr->IsMexInFinished(index)
			&& terrainMgr->CanMoveToPos(area, spots[index].position)
			/*&& (pos.SqDistance2D(spots[index].position) > minSqRange)*/)
		{
			return spots[index].position;
		}
	}
//	++scoutIdx %= scoutPath.size();
	return -RgtVector;
}

AIFloat3 CMilitaryManager::GetRaidPosition(CCircuitUnit* unit)
{
	// FIXME: Resume. Not well thought, not finished.
	return GetScoutPosition(unit);
	// FIXME: Resume. Not well thought, not finished.

//	const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
//	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
//	STerrainMapArea* area = unit->GetArea();
//	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
//	CThreatMap* threatMap = circuit->GetThreatMap();
//	threatMap->SetThreatType(unit);  // TODO: Check if required? Upper function may already call it
//	float bestWeight = -1.f;
//	float sqBestDist = std::numeric_limits<float>::max();
//	int bestIndex = -1;
//	for (size_t index = 0; index < raidPath.size(); ++index) {
//		if (!terrainMgr->CanMoveToPos(area, clusters[index].position)) {
//			continue;
//		}
//		const SRaidPoint& rp = raidPath[index];
//		float weight = rp.weight / (threatMap->GetThreatAt(clusters[index].position) + 1.f);
//		if (bestWeight < weight) {
//			bestWeight = weight;
//			bestIndex = index;
//			sqBestDist = pos.SqDistance2D(clusters[index].position);
//		} else if (rp.weight == bestWeight) {
//			float sqDist = pos.SqDistance2D(clusters[index].position);
//			if (sqBestDist > sqDist) {
//				sqBestDist = sqDist;
//				bestWeight = weight;
//				bestIndex = index;
//			}
//		}
//	}
//	return (bestIndex != -1) ? clusters[bestIndex].position : AIFloat3(-RgtVector);
}

void CMilitaryManager::FillFrontPos(CCircuitUnit* unit, F3Vec& outPositions)
{
	outPositions.clear();

	CInfluenceMap* inflMap = circuit->GetInflMap();
	CMetalManager* metalMgr = circuit->GetMetalManager();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	STerrainMapArea* area = unit->GetArea();
	const CMetalData::Clusters& clusters = metalMgr->GetClusters();

	CMetalData::PointPredicate predicate = [inflMap, metalMgr, terrainMgr, area, clusters](const int index) {
		return ((inflMap->GetInfluenceAt(clusters[index].position) > -INFL_EPS)
			&& (metalMgr->IsClusterQueued(index) || metalMgr->IsClusterFinished(index))
			&& terrainMgr->CanMoveToPos(area, clusters[index].position));
	};

	CSetupManager* setupMgr = circuit->GetSetupManager();
	int index = metalMgr->FindNearestCluster(setupMgr->GetLanePos(), predicate);

	if (index >= 0) {
		const std::vector<CDefenceMatrix::SDefPoint>& points = defence->GetDefPoints(index);
		for (const CDefenceMatrix::SDefPoint& defPoint : points) {
			outPositions.push_back(defPoint.position);
		}
	}
}

void CMilitaryManager::FillAttackSafePos(CCircuitUnit* unit, F3Vec& outPositions)
{
	outPositions.clear();

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	const int frame = circuit->GetLastFrame();

	STerrainMapArea* area = unit->GetArea();

	const std::array<IFighterTask::FightType, 2> types = {IFighterTask::FightType::ATTACK, IFighterTask::FightType::DEFEND};
	for (IFighterTask::FightType type : types) {
		const std::set<IFighterTask*>& atkTasks = GetTasks(type);
		for (IFighterTask* task : atkTasks) {
			const AIFloat3& ourPos = static_cast<ISquadTask*>(task)->GetLeaderPos(frame);
			if (terrainMgr->CanMoveToPos(area, ourPos)) {
				outPositions.push_back(ourPos);
			}
		}
	}
}

void CMilitaryManager::FillStaticSafePos(CCircuitUnit* unit, F3Vec& outPositions)
{
	outPositions.clear();

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	const int frame = circuit->GetLastFrame();

	const AIFloat3& startPos = unit->GetPos(frame);
	STerrainMapArea* area = unit->GetArea();

	CDefenceMatrix* defMat = defence;
	CMetalData::PointPredicate predicate = [defMat, terrainMgr, area](const int index) {
		const std::vector<CDefenceMatrix::SDefPoint>& points = defMat->GetDefPoints(index);
		for (const CDefenceMatrix::SDefPoint& defPoint : points) {
			if ((defPoint.cost > 100.0f) && terrainMgr->CanMoveToPos(area, defPoint.position)) {
				return true;
			}
		}
		return false;
	};

	CMetalManager* metalMgr = circuit->GetMetalManager();
	int index = metalMgr->FindNearestCluster(startPos, predicate);

	if (index >= 0) {
		const std::vector<CDefenceMatrix::SDefPoint>& points = defence->GetDefPoints(index);
		for (const CDefenceMatrix::SDefPoint& defPoint : points) {
			outPositions.push_back(defPoint.position);
		}
	}
}

void CMilitaryManager::FillSafePos(CCircuitUnit* unit, F3Vec& outPositions)
{
	outPositions.clear();

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	const int frame = circuit->GetLastFrame();

	const springai::AIFloat3& pos = unit->GetPos(frame);
	STerrainMapArea* area = unit->GetArea();

	const std::array<IFighterTask::FightType, 2> types = {IFighterTask::FightType::ATTACK, IFighterTask::FightType::DEFEND};
	for (IFighterTask::FightType type : types) {
		const std::set<IFighterTask*>& atkTasks = GetTasks(type);
		for (IFighterTask* task : atkTasks) {
			const AIFloat3& ourPos = static_cast<ISquadTask*>(task)->GetLeaderPos(frame);
			if (terrainMgr->CanMoveToPos(area, ourPos)) {
				outPositions.push_back(ourPos);
			}
		}
		if (!outPositions.empty()) {
			return;
		}
	}

	CDefenceMatrix* defMat = defence;
	CMetalData::PointPredicate predicate = [defMat, terrainMgr, area](const int index) {
		const std::vector<CDefenceMatrix::SDefPoint>& points = defMat->GetDefPoints(index);
		for (const CDefenceMatrix::SDefPoint& defPoint : points) {
			if ((defPoint.cost > 100.0f) && terrainMgr->CanMoveToPos(area, defPoint.position)) {
				return true;
			}
		}
		return false;
	};
	CMetalManager* metalMgr = circuit->GetMetalManager();
	int index = metalMgr->FindNearestCluster(pos, predicate);
	if (index >= 0) {
		const std::vector<CDefenceMatrix::SDefPoint>& points = defence->GetDefPoints(index);
		for (const CDefenceMatrix::SDefPoint& defPoint : points) {
			outPositions.push_back(defPoint.position);
		}
	}

	if (outPositions.empty()) {
		outPositions.push_back(circuit->GetSetupManager()->GetBasePos());
	}
}

IFighterTask* CMilitaryManager::AddGuardTask(CCircuitUnit* unit)
{
	auto it = guardTasks.find(unit);
	if (it != guardTasks.end()) {
		return it->second;
	}

	IFighterTask* task = EnqueueGuard(unit);
	guardTasks[unit] = task;
	return task;
}

bool CMilitaryManager::DelGuardTask(CCircuitUnit* unit)
{
	auto it = guardTasks.find(unit);
	if (it == guardTasks.end()) {
		return false;
	}

	AbortTask(it->second);
	guardTasks.erase(it);
	return true;
}

IFighterTask* CMilitaryManager::GetGuardTask(CCircuitUnit* unit) const
{
	auto it = guardTasks.find(unit);
	return (it != guardTasks.end()) ? it->second : nullptr;
}

void CMilitaryManager::AddResponse(CCircuitUnit* unit)
{
	const CCircuitDef* cdef = unit->GetCircuitDef();
	const float cost = cdef->GetCostM();
	const CCircuitDef::RoleT roleSize = CCircuitDef::GetRoleNames().size();
	assert(roleInfos.size() == roleSize);
	for (CCircuitDef::RoleT type = 0; type < roleSize; ++type) {
		if (cdef->IsRespRoleAny(CCircuitDef::GetMask(type))) {
			roleInfos[type].cost += cost;
			roleInfos[type].units.insert(unit);
		}
	}
}

void CMilitaryManager::DelResponse(CCircuitUnit* unit)
{
	const CCircuitDef* cdef = unit->GetCircuitDef();
	const float cost = cdef->GetCostM();
	const CCircuitDef::RoleT roleSize = CCircuitDef::GetRoleNames().size();
	assert(roleInfos.size() == roleSize);
	for (CCircuitDef::RoleT type = 0; type < roleSize; ++type) {
		if (cdef->IsRespRoleAny(CCircuitDef::GetMask(type))) {
			float& metal = roleInfos[type].cost;
			metal = std::max(metal - cost, .0f);
			roleInfos[type].units.erase(unit);
		}
	}
}

float CMilitaryManager::RoleProbability(const CCircuitDef* cdef) const
{
	CEnemyManager* enemyMgr = circuit->GetEnemyManager();
	const SRoleInfo& info = roleInfos[cdef->GetMainRole()];
	float maxProb = 0.f;
	for (const SRoleInfo::SVsInfo& vs : info.vs) {
		const float enemyMetal = enemyMgr->GetEnemyCost(vs.role);
		const float nextMetal = info.cost + cdef->GetCostM();
		const float prob = enemyMetal / (info.cost + 1.f) * vs.importance;
		if ((prob > maxProb) &&
			(enemyMetal * vs.ratio >= nextMetal * info.factor) &&
			(nextMetal <= (armyCost + cdef->GetCostM()) * info.maxPerc))
		{
			maxProb = prob;
		}
	}
	return maxProb;
}

bool CMilitaryManager::IsNeedBigGun(const CCircuitDef* cdef) const
{
	return armyCost * circuit->GetEconomyManager()->GetEcoFactor() > cdef->GetCostM();
}

AIFloat3 CMilitaryManager::GetBigGunPos(CCircuitDef* bigDef) const
{
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	AIFloat3 pos = circuit->GetSetupManager()->GetBasePos();
	if (bigDef->GetMaxRange() < std::max(terrainMgr->GetTerrainWidth(), terrainMgr->GetTerrainHeight())) {
		CMetalManager* metalMgr = circuit->GetMetalManager();
		const CMetalData::Clusters& clusters = metalMgr->GetClusters();
		unsigned size = 1;
		for (unsigned i = 0; i < clusters.size(); ++i) {
			if (metalMgr->IsClusterFinished(i)) {
				pos += clusters[i].position;
				++size;
			}
		}
		pos /= size;
	}
	return pos;
}

void CMilitaryManager::DiceBigGun()
{
	if (superInfos.empty()) {
		return;
	}

	std::vector<SSuperInfo> candidates;
	candidates.reserve(superInfos.size());
	float magnitude = 0.f;
	for (SSuperInfo& info : superInfos) {
		if (info.cdef->IsAvailable(circuit->GetLastFrame())) {
			candidates.push_back(info);
			magnitude += info.weight;
		}
	}
	if ((magnitude == 0.f) || candidates.empty()) {
		bigGunDef = superInfos[0].cdef;
		return;
	}

	unsigned choice = 0;
	float dice = (float)rand() / RAND_MAX * magnitude;
	for (unsigned i = 0; i < candidates.size(); ++i) {
		dice -= candidates[i].weight;
		if (dice < 0.f) {
			choice = i;
			break;
		}
	}
	bigGunDef = candidates[choice].cdef;
}

float CMilitaryManager::ClampMobileCostRatio() const
{
	const float enemyMobileCost = circuit->GetEnemyManager()->GetEnemyMobileCost();
	return (enemyMobileCost > armyCost) ? (armyCost / enemyMobileCost) : 1.f;
}

void CMilitaryManager::UpdateDefenceTasks()
{
	/*
	 * Defend expansion
	 */
	const std::set<IFighterTask*>& tasks = GetTasks(IFighterTask::FightType::DEFEND);
	CMetalManager* mm = circuit->GetMetalManager();
//	CEconomyManager* em = circuit->GetEconomyManager();
//	CTerrainManager* tm = circuit->GetTerrainManager();
//	const CMetalData::Metals& spots = mm->GetSpots();
	const CMetalData::Clusters& clusters = mm->GetClusters();
//	const std::vector<CEnemyManager::SEnemyGroup>& enemyGroups = circuit->GetEnemyManager()->GetEnemyGroups();
	for (IFighterTask* task : tasks) {
		CDefendTask* dt = static_cast<CDefendTask*>(task);
//		if (dt->GetTarget() != nullptr) {
//			continue;
//		}
//		STerrainMapArea* area = dt->GetLeader()->GetArea();
//		CMetalData::PointPredicate predicate = [em, tm, area, &spots, &clusters](const int index) {
//			const CMetalData::MetalIndices& idcs = clusters[index].idxSpots;
//			for (int idx : idcs) {
//				if (!em->IsOpenSpot(idx) && tm->CanMoveToPos(area, spots[idx].position)) {
//					return true;
//				}
//			}
//			return false;
//		};
//		AIFloat3 center(tm->GetTerrainWidth() / 2, 0, tm->GetTerrainHeight() / 2);
//		int index = mm->FindNearestCluster(center, predicate);
//		if (index >= 0) {
//			dt->SetPosition(clusters[index].position);
//		}

		if (dt->GetPromote() != IFighterTask::FightType::ATTACK) {
			continue;
		}
//		int groupIdx = -1;
//		float minSqDist = std::numeric_limits<float>::max();
//		const AIFloat3& position = dt->GetPosition();
//		for (unsigned i = 0; i < enemyGroups.size(); ++i) {
//			const CEnemyManager::SEnemyGroup& group = enemyGroups[i];
//			const float sqDist = position.SqDistance2D(group.pos);
//			if (sqDist < minSqDist) {
//				minSqDist = sqDist;
//				groupIdx = i;
//			}
//		}
//		if (groupIdx >= 0) {
//			dt->SetMaxPower(std::max(minAttackers, enemyGroups[groupIdx].threat));
//		}
		dt->SetMaxPower(std::max(minAttackers, circuit->GetEnemyManager()->GetMaxGroupThreat()));
	}

	/*
	 * Porc update
	 */
	decltype(defenceIdx) prevIdx = defenceIdx;
	while (defenceIdx < clusters.size()) {
		int index = defenceIdx++;
		if (mm->IsClusterQueued(index) || mm->IsClusterFinished(index)) {
			MakeDefence(index);
			return;
		}
	}
	defenceIdx = 0;
	while (defenceIdx < prevIdx) {
		int index = defenceIdx++;
		if (mm->IsClusterQueued(index) || mm->IsClusterFinished(index)) {
			MakeDefence(index);
			return;
		}
	}
}

void CMilitaryManager::UpdateDefence()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	const int frame = circuit->GetLastFrame();
	decltype(buildDefence)::iterator ibd = buildDefence.begin();
	while (ibd != buildDefence.end()) {
		const auto& defElem = ibd->second.back();
		if (frame >= defElem.second) {
			CCircuitDef* buildDef = defElem.first;
			if (buildDef->IsAvailable(frame)) {
				circuit->GetBuilderManager()->EnqueueTask(IBuilderTask::Priority::NORMAL, buildDef, ibd->first,
														  IBuilderTask::BuildType::DEFENCE, 0.f, true, 0);
			}
			ibd->second.pop_back();
		}
		if (ibd->second.empty()) {
			ibd = buildDefence.erase(ibd);
		} else {
			++ibd;
		}
	}
	if (buildDefence.empty()) {
		circuit->GetScheduler()->RemoveTask(defend);
		defend = nullptr;
	}
}

void CMilitaryManager::MakeBaseDefence(const AIFloat3& pos)
{
	if (baseDefence.empty() || (circuit->IsLoadSave() && (circuit->GetLastFrame() < 0))) {
		return;
	}
	buildDefence.push_back(std::make_pair(pos, baseDefence));
	if (defend == nullptr) {
		defend = std::make_shared<CGameTask>(&CMilitaryManager::UpdateDefence, this);
		circuit->GetScheduler()->RunTaskEvery(defend, FRAMES_PER_SEC);
	}
}

void CMilitaryManager::MarkPointOfInterest(CEnemyInfo* enemy)
{
	if (enemy->GetCircuitDef() != circuit->GetEconomyManager()->GetMexDef()) {  // TODO: if one of the list
		return;
	}
	int index = circuit->GetMetalManager()->FindNearestCluster(enemy->GetPos());
	SRaidPoint& rp = raidPath[index];
	rp.lastFrame = circuit->GetLastFrame();
	rp.units.insert(enemy);
	rp.weight = rp.units.size();  // TODO
}

void CMilitaryManager::UnmarkPointOfInterest(CEnemyInfo* enemy)
{
	if (enemy->GetCircuitDef() != circuit->GetEconomyManager()->GetMexDef()) {  // TODO: if one of the list
		return;
	}
	int index = circuit->GetMetalManager()->FindNearestCluster(enemy->GetPos());
	SRaidPoint& rp = raidPath[index];
	rp.lastFrame = circuit->GetLastFrame();
	rp.units.erase(enemy);
	rp.weight = rp.units.size();  // TODO
}

IUnitTask* CMilitaryManager::DefaultMakeTask(CCircuitUnit* unit)
{
	// FIXME: Make central task assignment system.
	//        MilitaryManager should decide what tasks to merge.
	static const std::map<CCircuitDef::RoleT, IFighterTask::FightType> types = {
		{ROLE_TYPE(SCOUT),   IFighterTask::FightType::SCOUT},
		{ROLE_TYPE(RAIDER),  IFighterTask::FightType::RAID},
		{ROLE_TYPE(RIOT),    IFighterTask::FightType::DEFEND},
		{ROLE_TYPE(ARTY),    IFighterTask::FightType::ARTY},
		{ROLE_TYPE(AA),      IFighterTask::FightType::AA},
		{ROLE_TYPE(AH),      IFighterTask::FightType::AH},
		{ROLE_TYPE(BOMBER),  IFighterTask::FightType::BOMB},
		{ROLE_TYPE(SUPPORT), IFighterTask::FightType::SUPPORT},
		{ROLE_TYPE(MINE),    IFighterTask::FightType::SCOUT},  // FIXME
		{ROLE_TYPE(SUPER),   IFighterTask::FightType::SUPER},
	};
	IFighterTask* task = nullptr;
	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->IsRoleSupport()) {
		if (/*cdef->IsAttacker() && */GetTasks(IFighterTask::FightType::ATTACK).empty() && GetTasks(IFighterTask::FightType::DEFEND).empty()) {
			task = EnqueueDefend(IFighterTask::FightType::ATTACK,
								 IFighterTask::FightType::SUPPORT, minAttackers);
		} else {
			task = EnqueueTask(IFighterTask::FightType::SUPPORT);
		}
	} else {
		auto it = types.find(circuit->GetBindedRole(cdef->GetMainRole()));
		if (it != types.end()) {
			switch (it->second) {
				case IFighterTask::FightType::RAID: {
					if (cdef->IsRoleScout() && (GetTasks(IFighterTask::FightType::SCOUT).size() < maxScouts)) {
						task = EnqueueTask(IFighterTask::FightType::SCOUT);
					} else {
						const std::set<IFighterTask*>& guards = GetTasks(IFighterTask::FightType::GUARD);
						for (IFighterTask* t : guards) {
							if (t->GetAssignees().empty()) {
								task = t;
								break;
							}
						}
						if (task == nullptr) {
							if (GetTasks(IFighterTask::FightType::RAID).empty()) {
								task = EnqueueDefend(IFighterTask::FightType::RAID, raid.min);
							}
						}
					}
				} break;
				case IFighterTask::FightType::AH: {
					if (!cdef->IsRoleMine() && (circuit->GetEnemyManager()->GetEnemyCost(ROLE_TYPE(HEAVY)) < 1.f)) {
						task = EnqueueTask(IFighterTask::FightType::ATTACK);
					}
				} break;
				case IFighterTask::FightType::DEFEND: {
					const std::set<IFighterTask*>& guards = GetTasks(IFighterTask::FightType::GUARD);
					for (IFighterTask* t : guards) {
						if (t->GetAssignees().empty()) {
							task = t;
							break;
						}
					}
					if (task == nullptr) {
						const float power = std::max(minAttackers, circuit->GetEnemyManager()->GetEnemyThreat() / circuit->GetAllyTeam()->GetAliveSize());
						task = EnqueueDefend(IFighterTask::FightType::ATTACK, power);
					}
				} break;
				default: break;
			}
			if (task == nullptr) {
				task = EnqueueTask(it->second);
			}
		} else {
			const bool isDefend = GetTasks(IFighterTask::FightType::ATTACK).empty();
			const float power = std::max(minAttackers, circuit->GetEnemyManager()->GetEnemyThreat() / circuit->GetAllyTeam()->GetAliveSize());
			task = isDefend ? EnqueueDefend(IFighterTask::FightType::ATTACK, power)
							: EnqueueTask(IFighterTask::FightType::ATTACK);
		}
	}

	return task;
}

void CMilitaryManager::Watchdog()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	for (CCircuitUnit* unit : army) {
		if (unit->GetTask()->GetType() == IUnitTask::Type::PLAYER) {
			continue;
		}
		auto commands = unit->GetUnit()->GetCurrentCommands();
		if (commands.empty()) {
			UnitIdle(unit);
		}
		utils::free_clear(commands);
	}
}

void CMilitaryManager::UpdateIdle()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	idleTask->Update();
}

void CMilitaryManager::UpdateFight()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	if (fightIterator >= fightUpdates.size()) {
		fightIterator = 0;
	}

	// stagger the Update's
	unsigned int n = (fightUpdates.size() / TEAM_SLOWUPDATE_RATE) + 1;

	while ((fightIterator < fightUpdates.size()) && (n != 0)) {
		IUnitTask* task = fightUpdates[fightIterator];
		if (task->IsDead()) {
			fightUpdates[fightIterator] = fightUpdates.back();
			fightUpdates.pop_back();
			task->ClearRelease();  // delete task;
		} else {
			task->Update();
			++fightIterator;
			n--;
		}
	}
}

void CMilitaryManager::AddArmyCost(CCircuitUnit* unit)
{
	AddResponse(unit);
	armyCost += unit->GetCircuitDef()->GetCostM();
}

void CMilitaryManager::DelArmyCost(CCircuitUnit* unit)
{
	DelResponse(unit);
	armyCost = std::max(armyCost - unit->GetCircuitDef()->GetCostM(), .0f);
}

} // namespace circuit
