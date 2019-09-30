/*
 * MilitaryManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "module/MilitaryManager.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
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
#include "terrain/ThreatMap.h"
#include "terrain/PathFinder.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#include "json/json.h"

#include "OOAICallback.h"
#include "AISCommands.h"
#include "Command.h"
#include "Log.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CMilitaryManager::CMilitaryManager(CCircuitAI* circuit)
		: IUnitModule(circuit)
		, fightIterator(0)
		, defenceIdx(0)
		, scoutIdx(0)
		, armyCost(0.f)
		, enemyMobileCost(0.f)
		, mobileThreat(0.f)
		, staticThreat(0.f)
		, radarDef(nullptr)
		, sonarDef(nullptr)
		, bigGunDef(nullptr)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::Watchdog, this),
							FRAMES_PER_SEC * 60,
							circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 12);
	scheduler->RunTaskAt(std::make_shared<CGameTask>(&CMilitaryManager::Init, this));

	/*
	 * Defence handlers
	 */
	auto defenceFinishedHandler = [this](CCircuitUnit* unit) {
		TRY_UNIT(this->circuit, unit,
			unit->GetUnit()->Stockpile(UNIT_COMMAND_OPTION_SHIFT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
		)
	};
	auto defenceDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		int frame = this->circuit->GetLastFrame();
		float defCost = unit->GetCircuitDef()->GetCost();
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
					unit->GetUnit()->ExecuteCustomCommand(CMD_AIR_STRAFE, {0.0f});
				}
				if (unit->GetCircuitDef()->IsRoleMine()) {
					unit->GetUnit()->SetIdleMode(1);
				}
			}
			if (unit->GetCircuitDef()->IsAttrStock()) {
				unit->GetUnit()->Stockpile(UNIT_COMMAND_OPTION_SHIFT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
				unit->GetUnit()->ExecuteCustomCommand(CMD_MISC_PRIORITY, {2.0f});
			}
		)
	};
	auto attackerIdleHandler = [this](CCircuitUnit* unit) {
		// NOTE: Avoid instant task reassignment, though it may be not relevant for attackers
		if (this->circuit->GetLastFrame() > unit->GetTaskFrame()/* + FRAMES_PER_SEC*/) {
			unit->GetTask()->OnUnitIdle(unit);
		}
	};
	auto attackerDamagedHandler = [](CCircuitUnit* unit, CEnemyUnit* attacker) {
		unit->GetTask()->OnUnitDamaged(unit, attacker);
	};
	auto attackerDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
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
				unit->GetUnit()->ExecuteCustomCommand(CMD_MISC_PRIORITY, {2.0f});
			}
		)
	};
	auto superDestroyedHandler = [](CCircuitUnit* unit, CEnemyUnit* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask
	};

	/*
	 * Defend buildings handler
	 */
	auto structDamagedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		const std::set<IFighterTask*>& tasks = GetTasks(IFighterTask::FightType::DEFEND);
		if (tasks.empty()) {
			return;
		}
		int frame = this->circuit->GetLastFrame();
		const AIFloat3& pos = unit->GetPos(frame);
		CTerrainManager* terrainManager = this->circuit->GetTerrainManager();
		CDefendTask* defendTask = nullptr;
		float minSqDist = std::numeric_limits<float>::max();
		for (IFighterTask* task : tasks) {
			CDefendTask* dt = static_cast<CDefendTask*>(task);
			const float sqDist = pos.SqDistance2D(dt->GetPosition());
			if ((minSqDist <= sqDist) || !terrainManager->CanMoveToPos(dt->GetLeader()->GetArea(), pos)) {
				continue;
			}
			if ((dt->GetTarget() == nullptr) ||
				(dt->GetTarget()->GetPos().SqDistance2D(dt->GetLeader()->GetPos(frame)) > sqDist))
			{
				minSqDist = sqDist;
				defendTask = dt;
			}
		}
		if (defendTask != nullptr) {
			defendTask->SetPosition(pos);
			defendTask->SetWantedTarget(attacker);
		}
	};

	// NOTE: IsRole used below
	ReadConfig();

	for (const CCircuitDef* cdef : defenderDefs) {
		destroyedHandler[cdef->GetId()] = defenceDestroyedHandler;
	}

	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const float fighterRet = root["retreat"].get("fighter", 0.5f).asFloat();
	const float commMod = root["quota"]["thr_mod"].get("comm", 1.f).asFloat();
	float maxRadarDivCost = 0.f;
	float maxSonarDivCost = 0.f;
	CCircuitDef* commDef = circuit->GetSetupManager()->GetCommChoice();

	const CCircuitAI::CircuitDefs& allDefs = circuit->GetCircuitDefs();
	for (auto& kv : allDefs) {
		CCircuitDef::Id unitDefId = kv.first;
		CCircuitDef* cdef = kv.second;
		if (cdef->IsRoleComm()) {
			cdef->ModThreat(commMod);
		}
		if (cdef->GetUnitDef()->IsBuilder()) {
			damagedHandler[unitDefId] = structDamagedHandler;
			continue;
		}
		const std::map<std::string, std::string>& customParams = cdef->GetUnitDef()->GetCustomParams();
		auto it = customParams.find("is_drone");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			continue;
		}
		if (cdef->IsMobile()) {
			createdHandler[unitDefId] = attackerCreatedHandler;
			finishedHandler[unitDefId] = attackerFinishedHandler;
			idleHandler[unitDefId] = attackerIdleHandler;
			damagedHandler[unitDefId] = attackerDamagedHandler;
			destroyedHandler[unitDefId] = attackerDestroyedHandler;

			if (cdef->GetRetreat() < 0.f) {
				cdef->SetRetreat(fighterRet);
			}
		} else {
			damagedHandler[unitDefId] = structDamagedHandler;
			if (cdef->IsAttacker()) {
				if (cdef->IsRoleSuper()) {
					createdHandler[unitDefId] = superCreatedHandler;
					finishedHandler[unitDefId] = superFinishedHandler;
					destroyedHandler[unitDefId] = superDestroyedHandler;
				} else if (cdef->IsAttrStock()) {
					finishedHandler[unitDefId] = defenceFinishedHandler;
				}
			}
			if (commDef->CanBuild(cdef)) {
				float range = cdef->GetUnitDef()->GetRadarRadius();
				float areaDivCost = M_PI * SQUARE(range) / cdef->GetCost();
				if (maxRadarDivCost < areaDivCost) {
					maxRadarDivCost = areaDivCost;
					radarDef = cdef;
				}
				range = cdef->GetUnitDef()->GetSonarRadius();
				areaDivCost = M_PI * SQUARE(range) / cdef->GetCost();
				if (maxSonarDivCost < areaDivCost) {
					maxSonarDivCost = areaDivCost;
					sonarDef = cdef;
				}
			}
		}
	}

	defence = circuit->GetAllyTeam()->GetDefenceMatrix().get();

	fightTasks.resize(static_cast<IFighterTask::FT>(IFighterTask::FightType::_SIZE_));

	enemyPos = AIFloat3(circuit->GetTerrainManager()->GetTerrainWidth() / 2, 0, circuit->GetTerrainManager()->GetTerrainHeight() / 2);
	enemyGroups.push_back(SEnemyGroup(enemyPos));
}

CMilitaryManager::~CMilitaryManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(fightUpdates);
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

int CMilitaryManager::UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	auto search = damagedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != damagedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
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
//			CEconomyManager* economyManager = circuit->GetEconomyManager();
//			float power = economyManager->GetAvgMetalIncome() * economyManager->GetEcoFactor() * 32.0f;
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
	IFighterTask* task = new CDefendTask(this, circuit->GetSetupManager()->GetBasePos(), defRadius,
										 promote, promote, power, 1.0f / mod);
	fightTasks[static_cast<IFighterTask::FT>(IFighterTask::FightType::DEFEND)].insert(task);
	fightUpdates.push_back(task);
	return task;
}

IFighterTask* CMilitaryManager::EnqueueDefend(IFighterTask::FightType check, IFighterTask::FightType promote)
{
	IFighterTask* task = new CDefendTask(this, circuit->GetSetupManager()->GetBasePos(), defRadius,
										 check, promote, std::numeric_limits<float>::max(), 1.0f);
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

void CMilitaryManager::DequeueTask(IFighterTask* task, bool done)
{
	if (task->GetType() == IUnitTask::Type::FIGHTER) {
		fightTasks[static_cast<IFighterTask::FT>(task->GetFightType())].erase(task);
	}
	task->Dead();
	task->Close(done);
}

IUnitTask* CMilitaryManager::MakeTask(CCircuitUnit* unit)
{
	// FIXME: Make central task assignment system.
	//        MilitaryManager should decide what tasks to merge.
	static const std::map<CCircuitDef::RoleT, IFighterTask::FightType> types = {
		{static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::SCOUT),   IFighterTask::FightType::SCOUT},
		{static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::RAIDER),  IFighterTask::FightType::RAID},
		{static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::ARTY),    IFighterTask::FightType::ARTY},
		{static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::AA),      IFighterTask::FightType::AA},
		{static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::AH),      IFighterTask::FightType::AH},
		{static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::BOMBER),  IFighterTask::FightType::BOMB},
		{static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::SUPPORT), IFighterTask::FightType::SUPPORT},
		{static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::MINE),    IFighterTask::FightType::SCOUT},  // FIXME
		{static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::SUPER),   IFighterTask::FightType::SUPER},
	};
	IFighterTask* task = nullptr;
	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->IsRoleSupport()) {
		if (/*cdef->IsAttacker() && */GetTasks(IFighterTask::FightType::ATTACK).empty()) {
			task = EnqueueDefend(IFighterTask::FightType::ATTACK,
								 IFighterTask::FightType::SUPPORT);
		} else {
			task = EnqueueTask(IFighterTask::FightType::SUPPORT);
		}
	} else {
		auto it = types.find(cdef->GetMainRole());
		if (it != types.end()) {
			switch (it->second) {
				case IFighterTask::FightType::RAID: {
					if (cdef->IsRoleScout() && (GetTasks(IFighterTask::FightType::SCOUT).size() < maxScouts)) {
						task = EnqueueTask(IFighterTask::FightType::SCOUT);
					} else if (GetTasks(IFighterTask::FightType::RAID).empty()) {
						task = EnqueueDefend(IFighterTask::FightType::RAID, raid.min);
					}
				} break;
				case IFighterTask::FightType::AH: {
					if (!cdef->IsRoleMine() && (GetEnemyCost(CCircuitDef::RoleType::HEAVY) < 1.f)) {
						task = EnqueueTask(IFighterTask::FightType::ATTACK);
					}
				} break;
				default: break;
			}
			if (task == nullptr) {
				task = EnqueueTask(it->second);
			}
		} else {
			const bool isDefend = GetTasks(IFighterTask::FightType::ATTACK).empty();
			const float power = std::max(minAttackers, GetEnemyThreat() / circuit->GetAllyTeam()->GetAliveSize());
			task = isDefend ? EnqueueDefend(IFighterTask::FightType::ATTACK, power)
							: EnqueueTask(IFighterTask::FightType::ATTACK);
		}
	}

	return task;
}

void CMilitaryManager::AbortTask(IUnitTask* task)
{
	DequeueTask(static_cast<IFighterTask*>(task), false);
}

void CMilitaryManager::DoneTask(IUnitTask* task)
{
	DequeueTask(static_cast<IFighterTask*>(task), true);
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
	CBuilderManager* builderManager = circuit->GetBuilderManager();
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
		const CMetalData::Graph& clusterGraph = mm->GetGraph();
		CMetalData::Graph::Node node = clusterGraph.nodeFromId(cluster);
		CMetalData::Graph::IncEdgeIt edgeIt(clusterGraph, node);
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
		for (IBuilderTask* t : builderManager->GetTasks(IBuilderTask::BuildType::DEFENCE)) {
			if ((t->GetTarget() == nullptr) && (t->GetNextTask() != nullptr) &&
				(closestPoint->position.SqDistance2D(t->GetTaskPos()) < SQUARE(SQUARE_SIZE)))
			{
				builderManager->AbortTask(t);
				break;
			}
		}
	}
	unsigned num = std::min<unsigned>(isPorc ? defenders.size() : preventCount, defenders.size());

	AIFloat3 backDir = circuit->GetSetupManager()->GetBasePos() - closestPoint->position;
	AIFloat3 backPos = closestPoint->position + backDir.Normalize2D() * SQUARE_SIZE * 16;
	CTerrainManager::CorrectPosition(backPos);

	const int frame = circuit->GetLastFrame();
	for (unsigned i = 0; i < num; ++i) {
		CCircuitDef* defDef = defenders[i];
		if (!defDef->IsAvailable(frame) || (defDef->IsRoleAA() && (GetEnemyCost(CCircuitDef::RoleType::AIR) < 1.f))) {
			continue;
		}
		float defCost = defDef->GetCost();
		totalCost += defCost;
		if (totalCost <= closestPoint->cost) {
			continue;
		}
		if (totalCost < maxCost) {
			closestPoint->cost += defCost;
			bool isFirst = (parentTask == nullptr);
			const AIFloat3& buildPos = defDef->IsAttacker() ? closestPoint->position : backPos;
			IBuilderTask* task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, defDef, buildPos,
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
	auto checkSensor = [this, &backPos, builderManager](IBuilderTask::BuildType type, CCircuitDef* cdef, float range) {
		bool isBuilt = false;
		auto friendlies = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(backPos, range));
		for (Unit* au : friendlies) {
			if (au == nullptr) {
				continue;
			}
			UnitDef* udef = au->GetDef();
			CCircuitDef::Id defId = udef->GetUnitDefId();
			delete udef;
			if (defId == cdef->GetId()) {
				isBuilt = true;
				break;
			}
		}
		utils::free_clear(friendlies);
		if (!isBuilt) {
			const IBuilderTask* task = nullptr;
			const float qdist = range * range;
			for (const IBuilderTask* t : builderManager->GetTasks(type)) {
				if (backPos.SqDistance2D(t->GetTaskPos()) < qdist) {
					task = t;
					break;
				}
			}
			if (task == nullptr) {
				builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, backPos, type);
			}
		}
	};
	// radar
	if ((radarDef != nullptr) && radarDef->IsAvailable(frame) && (radarDef->GetCost() < maxCost)) {
		const float range = radarDef->GetUnitDef()->GetRadarRadius() / (isPorc ? 4.f : SQRT_2);
		checkSensor(IBuilderTask::BuildType::RADAR, radarDef, range);
	}
	// sonar
	if (isWater && (sonarDef != nullptr) && sonarDef->IsAvailable(frame) && (sonarDef->GetCost() < maxCost)) {
		checkSensor(IBuilderTask::BuildType::SONAR, sonarDef, sonarDef->GetUnitDef()->GetSonarRadius());
	}
}

void CMilitaryManager::AbortDefence(const CBDefenceTask* task)
{
	float defCost = task->GetBuildDef()->GetCost();
	CDefenceMatrix::SDefPoint* point = defence->GetDefPoint(task->GetPosition(), defCost);
	if (point != nullptr) {
		if ((task->GetTarget() == nullptr) && (point->cost >= defCost)) {
			point->cost -= defCost;
		}
		IBuilderTask* next = task->GetNextTask();
		while (next != nullptr) {
			if (next->GetBuildDef() != nullptr) {
				defCost = next->GetBuildDef()->GetCost();
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
	// FIXME: Resume fighter/DefendTask experiment
	return true;
	// FIXME: Resume fighter/DefendTask experiment

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
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CMetalManager* metalManager = circuit->GetMetalManager();
	STerrainMapArea* area = unit->GetArea();
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	const float minSqRange = SQUARE(unit->GetCircuitDef()->GetLosRadius());
	const CMetalData::Metals& spots = metalManager->GetSpots();
	decltype(scoutIdx) prevIdx = scoutIdx;
	while (scoutIdx < scoutPath.size()) {
		int index = scoutPath[scoutIdx++];
		if (!metalManager->IsMexInFinished(index) &&
			terrainManager->CanMoveToPos(area, spots[index].position) &&
			(pos.SqDistance2D(spots[index].position) > minSqRange))
		{
			return spots[index].position;
		}
	}
	scoutIdx = 0;
	while (scoutIdx < prevIdx) {
		int index = scoutPath[scoutIdx++];
		if (!metalManager->IsMexInFinished(index) &&
			terrainManager->CanMoveToPos(area, spots[index].position) &&
			(pos.SqDistance2D(spots[index].position) > minSqRange))
		{
			return spots[index].position;
		}
	}
//	++scoutIdx %= scoutPath.size();
	return -RgtVector;
}

void CMilitaryManager::FindBestPos(F3Vec& posPath, AIFloat3& startPos, STerrainMapArea* area)
{
	static F3Vec ourPositions;  // NOTE: micro-opt

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CPathFinder* pathfinder = circuit->GetPathfinder();
	const int frame = circuit->GetLastFrame();

	/*
	 * Check mobile groups
	 */
	const std::array<IFighterTask::FightType, 2> types = {IFighterTask::FightType::ATTACK, IFighterTask::FightType::DEFEND};
	for (IFighterTask::FightType type : types) {
		const std::set<IFighterTask*>& atkTasks = GetTasks(type);
		for (IFighterTask* task : atkTasks) {
			const AIFloat3& ourPos = static_cast<ISquadTask*>(task)->GetLeaderPos(frame);
			if (terrainManager->CanMoveToPos(area, ourPos)) {
				ourPositions.push_back(ourPos);
			}
		}

		if (!ourPositions.empty()) {
			pathfinder->FindBestPath(posPath, startPos, pathfinder->GetSquareSize(), ourPositions, false);
			ourPositions.clear();
			if (!posPath.empty()) {
				return;
			}
		}
	}

	/*
	 * Check static cluster defences
	 */
//	unsigned threatCount = 0;
//	unsigned clusterCount = 0;
//	CThreatMap* threatMap = circuit->GetThreatMap();
//	CMetalManager* mm = circuit->GetMetalManager();
//	const CMetalData::Clusters& clusters = mm->GetClusters();
//	const CMetalData::Metals& spots = mm->GetSpots();
//	const CMetalData::Graph& clusterGraph = mm->GetGraph();
//	CMetalData::Graph::out_edge_iterator outEdgeIt, outEdgeEnd;
//	std::tie(outEdgeIt, outEdgeEnd) = boost::out_edges(cluster, clusterGraph);
//	for (; (outEdgeIt != outEdgeEnd); ++outEdgeIt) {
//		const CMetalData::EdgeDesc& edgeId = *outEdgeIt;
//		int idx0 = boost::target(edgeId, clusterGraph);
//		if (mm->IsClusterFinished(idx0)) {
//			continue;
//		}
//		// check if there is enemy neighbor
//		for (int idx : clusters[idx0].idxSpots) {
//			if (threatMap->GetAllThreatAt(spots[idx].position) > THREAT_MIN * 2) {
//				threatCount++;
//				break;
//			}
//		}
//		if (threatCount >= 2) {  // if 2 nearby clusters are a threat
//			threatCount = 0;
//			if (++clusterCount >= 3) {
//				break;
//			}
//		}
//	}

	CDefenceMatrix* defMat = defence;
	CMetalData::PointPredicate predicate = [defMat, terrainManager, area](const int index) {
		const std::vector<CDefenceMatrix::SDefPoint>& points = defMat->GetDefPoints(index);
		for (const CDefenceMatrix::SDefPoint& defPoint : points) {
			if ((defPoint.cost > 100.0f) && terrainManager->CanMoveToPos(area, defPoint.position)) {
				return true;
			}
		}
		return false;
	};
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(startPos, predicate);
	if (index >= 0) {
		const std::vector<CDefenceMatrix::SDefPoint>& points = defence->GetDefPoints(index);
		for (const CDefenceMatrix::SDefPoint& defPoint : points) {
			ourPositions.push_back(defPoint.position);
		}

		pathfinder->FindBestPath(posPath, startPos, pathfinder->GetSquareSize(), ourPositions, false);
		ourPositions.clear();
		if (!posPath.empty()) {
			return;
		}
	}

	/*
	 * Use base
	 */
	ourPositions.push_back(circuit->GetSetupManager()->GetBasePos());
	pathfinder->FindBestPath(posPath, startPos, pathfinder->GetSquareSize(), ourPositions, false);
	ourPositions.clear();
}

void CMilitaryManager::FillSafePos(const AIFloat3& pos, STerrainMapArea* area, F3Vec& outPositions)
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	const int frame = circuit->GetLastFrame();

	const std::array<IFighterTask::FightType, 2> types = {IFighterTask::FightType::ATTACK, IFighterTask::FightType::DEFEND};
	for (IFighterTask::FightType type : types) {
		const std::set<IFighterTask*>& atkTasks = GetTasks(type);
		for (IFighterTask* task : atkTasks) {
			const AIFloat3& ourPos = static_cast<ISquadTask*>(task)->GetLeaderPos(frame);
			if (terrainManager->CanMoveToPos(area, ourPos)) {
				outPositions.push_back(ourPos);
			}
		}
		if (!outPositions.empty()) {
			return;
		}
	}

	CDefenceMatrix* defMat = defence;
	CMetalData::PointPredicate predicate = [defMat, terrainManager, area](const int index) {
		const std::vector<CDefenceMatrix::SDefPoint>& points = defMat->GetDefPoints(index);
		for (const CDefenceMatrix::SDefPoint& defPoint : points) {
			if ((defPoint.cost > 100.0f) && terrainManager->CanMoveToPos(area, defPoint.position)) {
				return true;
			}
		}
		return false;
	};
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(pos, predicate);
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

IFighterTask* CMilitaryManager::AddDefendTask(int cluster)
{
	// FIXME: Resume fighter/DefendTask experiment
	return nullptr;
	// FIXME: Resume fighter/DefendTask experiment

//	IFighterTask* task = clusterInfos[cluster].defence;
//	if (task != nullptr) {
//		return task;
//	}
//
//	const AIFloat3& pos = circuit->GetMetalManager()->GetClusters()[cluster].geoCentr;
////	task = EnqueueTask(IFighterTask::FightType::DEFEND, pos, 1);
//	task = new CGuardTask(this, pos, 1);
//	fightTasks[static_cast<IFighterTask::FT>(IFighterTask::FightType::DEFEND)].insert(task);
//	clusterInfos[cluster].defence = task;
//	return task;
}

IFighterTask* CMilitaryManager::DelDefendTask(const AIFloat3& pos)
{
	// FIXME: Resume fighter/DefendTask experiment
	return nullptr;
	// FIXME: Resume fighter/DefendTask experiment

//	int index = circuit->GetMetalManager()->FindNearestCluster(pos);
//	if (index < 0) {
//		return nullptr;
//	}
//
//	return DelDefendTask(index);
}

IFighterTask* CMilitaryManager::DelDefendTask(int cluster)
{
	// FIXME: Resume fighter/DefendTask experiment
	return nullptr;
	// FIXME: Resume fighter/DefendTask experiment

//	IFighterTask* task = clusterInfos[cluster].defence;
//	if (task == nullptr) {
//		return nullptr;
//	}
//
//	clusterInfos[cluster].defence = nullptr;
//	return task;
}

void CMilitaryManager::AddEnemyCost(const CEnemyUnit* e)
{
	CCircuitDef* cdef = e->GetCircuitDef();
	assert(cdef != nullptr);

	for (CCircuitDef::RoleT i = 0; i < static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_); ++i) {
		if (cdef->IsEnemyRoleAny(CCircuitDef::GetMask(i))) {
			SEnemyInfo& info = enemyInfos[i];
			info.cost   += e->GetCost();
			info.threat += cdef->GetThreat();
		}
	}
	if (cdef->IsMobile()) {
		mobileThreat += cdef->GetThreat() * initThrMod.inMobile;
		enemyMobileCost += e->GetCost();
	} else {
		staticThreat += cdef->GetThreat() * initThrMod.inStatic;
	}
}

void CMilitaryManager::DelEnemyCost(const CEnemyUnit* e)
{
	CCircuitDef* cdef = e->GetCircuitDef();
	assert(cdef != nullptr);

	for (CCircuitDef::RoleT i = 0; i < static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_); ++i) {
		if (cdef->IsEnemyRoleAny(CCircuitDef::GetMask(i))) {
			SEnemyInfo& info = enemyInfos[i];
			info.cost   = std::max(info.cost   - e->GetCost(),      0.f);
			info.threat = std::max(info.threat - cdef->GetThreat(), 0.f);
		}
	}
	if (cdef->IsMobile()) {
		mobileThreat = std::max(mobileThreat - cdef->GetThreat() * initThrMod.inMobile, 0.f);
		enemyMobileCost = std::max(enemyMobileCost - e->GetCost(), 0.f);
	} else {
		staticThreat = std::max(staticThreat - cdef->GetThreat() * initThrMod.inStatic, 0.f);
	}
}

void CMilitaryManager::AddResponse(CCircuitUnit* unit)
{
	const CCircuitDef* cdef = unit->GetCircuitDef();
	const float cost = cdef->GetCost();
	assert(roleInfos.size() == static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_));
	for (CCircuitDef::RoleT i = 0; i < static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_); ++i) {
		if (cdef->IsRoleAny(CCircuitDef::GetMask(i))) {
			roleInfos[i].cost += cost;
			roleInfos[i].units.insert(unit);
		}
	}
}

void CMilitaryManager::DelResponse(CCircuitUnit* unit)
{
	const CCircuitDef* cdef = unit->GetCircuitDef();
	const float cost = cdef->GetCost();
	assert(roleInfos.size() == static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_));
	for (CCircuitDef::RoleT i = 0; i < static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_); ++i) {
		if (cdef->IsRoleAny(CCircuitDef::GetMask(i))) {
			float& metal = roleInfos[i].cost;
			metal = std::max(metal - cost, .0f);
			roleInfos[i].units.erase(unit);
		}
	}
}

float CMilitaryManager::RoleProbability(const CCircuitDef* cdef) const
{
	const SRoleInfo& info = roleInfos[cdef->GetMainRole()];
	float maxProb = 0.f;
	for (const SRoleInfo::SVsInfo& vs : info.vs) {
		const float enemyMetal = GetEnemyCost(vs.role);
		const float nextMetal = info.cost + cdef->GetCost();
		const float prob = enemyMetal / (info.cost + 1.f) * vs.importance;
		if ((prob > maxProb) &&
			(enemyMetal * vs.ratio >= nextMetal * info.factor) &&
			(nextMetal <= (armyCost + cdef->GetCost()) * info.maxPerc))
		{
			maxProb = prob;
		}
	}
	return maxProb;
}

bool CMilitaryManager::IsNeedBigGun(const CCircuitDef* cdef) const
{
	return armyCost * circuit->GetEconomyManager()->GetEcoFactor() > cdef->GetCost();
}

AIFloat3 CMilitaryManager::GetBigGunPos(CCircuitDef* bigDef) const
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	AIFloat3 pos = circuit->GetSetupManager()->GetBasePos();
	if (bigDef->GetMaxRange() < std::max(terrainManager->GetTerrainWidth(), terrainManager->GetTerrainHeight())) {
		CMetalManager* metalManager = circuit->GetMetalManager();
		const CMetalData::Clusters& clusters = metalManager->GetClusters();
		unsigned size = 1;
		for (unsigned i = 0; i < clusters.size(); ++i) {
			if (metalManager->IsClusterFinished(i)) {
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
		if (info.cdef->IsAvailable()) {
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

void CMilitaryManager::UpdateDefenceTasks()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	/*
	 * Defend expansion
	 */
	const std::set<IFighterTask*>& tasks = GetTasks(IFighterTask::FightType::DEFEND);
	CMetalManager* mm = circuit->GetMetalManager();
	CEconomyManager* em = circuit->GetEconomyManager();
	CTerrainManager* tm = circuit->GetTerrainManager();
	const CMetalData::Metals& spots = mm->GetSpots();
	const CMetalData::Clusters& clusters = mm->GetClusters();
	for (IFighterTask* task : tasks) {
		CDefendTask* dt = static_cast<CDefendTask*>(task);
		if (dt->GetTarget() != nullptr) {
			continue;
		}
		STerrainMapArea* area = dt->GetLeader()->GetArea();
		CMetalData::PointPredicate predicate = [em, tm, area, &spots, &clusters](const int index) {
			const CMetalData::MetalIndices& idcs = clusters[index].idxSpots;
			for (int idx : idcs) {
				if (!em->IsOpenSpot(idx) && tm->CanMoveToPos(area, spots[idx].position)) {
					return true;
				}
			}
			return false;
		};
		AIFloat3 center(tm->GetTerrainWidth() / 2, 0, tm->GetTerrainHeight() / 2);
		int index = mm->FindNearestCluster(center, predicate);
		if (index >= 0) {
			dt->SetPosition(clusters[index].position);
		}

		if (dt->GetPromote() != IFighterTask::FightType::ATTACK) {
			continue;
		}
		int groupIdx = -1;
		float minSqDist = std::numeric_limits<float>::max();
		const AIFloat3& position = dt->GetPosition();
		for (unsigned i = 0; i < enemyGroups.size(); ++i) {
			const CMilitaryManager::SEnemyGroup& group = enemyGroups[i];
			const float sqDist = position.SqDistance2D(group.pos);
			if (sqDist < minSqDist) {
				minSqDist = sqDist;
				groupIdx = i;
			}
		}
		if (groupIdx >= 0) {
			dt->SetMaxPower(std::max(minAttackers, enemyGroups[groupIdx].threat));
		}
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
	if (baseDefence.empty()) {
		return;
	}
	buildDefence.push_back(std::make_pair(pos, baseDefence));
	if (defend == nullptr) {
		defend = std::make_shared<CGameTask>(&CMilitaryManager::UpdateDefence, this);
		circuit->GetScheduler()->RunTaskEvery(defend, FRAMES_PER_SEC);
	}
}

void CMilitaryManager::ReadConfig()
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();
	CCircuitDef::RoleName& roleNames = CCircuitDef::GetRoleNames();

	const Json::Value& responses = root["response"];
	const float teamSize = circuit->GetAllyTeam()->GetSize();
	roleInfos.resize(static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_), {.0f});
	for (const auto& pair : roleNames) {
		SRoleInfo& info = roleInfos[static_cast<CCircuitDef::RoleT>(pair.second)];
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
			info.vs.push_back(SRoleInfo::SVsInfo(roleNames[roleName], rat, imp));
		}
	}

	const Json::Value& quotas = root["quota"];
	maxScouts = quotas.get("scout", 3).asUInt();
	const Json::Value& qraid = quotas["raid"];
	raid.min = qraid.get((unsigned)0, 3.f).asFloat();
	raid.avg = qraid.get((unsigned)1, 5.f).asFloat();
	minAttackers = quotas.get("attack", 8.f).asFloat();
	defRadius = quotas.get("def_rad", 2000.f).asFloat();
	const Json::Value& qthrMod = quotas["thr_mod"];
	const Json::Value& qthrAtk = qthrMod["attack"];
	attackMod.min = qthrAtk.get((unsigned)0, 1.f).asFloat();
	attackMod.len = qthrAtk.get((unsigned)1, 1.f).asFloat() - attackMod.min;
	const Json::Value& qthrDef = qthrMod["defence"];
	defenceMod.min = qthrDef.get((unsigned)0, 1.f).asFloat();
	defenceMod.len = qthrDef.get((unsigned)1, 1.f).asFloat() - defenceMod.min;
	initThrMod.inMobile = qthrMod.get("mobile", 1.f).asFloat();
	initThrMod.inStatic = qthrMod.get("static", 0.f).asFloat();
	maxAAThreat = quotas.get("aa_threat", 42.f).asFloat();

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
		si.cdef->SetMainRole(CCircuitDef::RoleType::SUPER);  // override mainRole
		si.cdef->AddEnemyRole(CCircuitDef::RoleType::SUPER);
		si.cdef->AddRole(CCircuitDef::RoleType::SUPER);
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
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Metals& spots = metalManager->GetSpots();

	clusterInfos.resize(metalManager->GetClusters().size(), {nullptr});

	scoutPath.reserve(spots.size());
	for (unsigned i = 0; i < spots.size(); ++i) {
		scoutPath.push_back(i);
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
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::UpdateFight, this), interval / 2, offset + 1);
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::UpdateDefenceTasks, this), FRAMES_PER_SEC * 5, offset + 2);
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
	fightUpdates.clear();
}

void CMilitaryManager::Watchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	for (CCircuitUnit* unit : army) {
		if (unit->GetTask()->GetType() == IUnitTask::Type::PLAYER) {
			continue;
		}
		auto commands = std::move(unit->GetUnit()->GetCurrentCommands());
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
			delete task;
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
	armyCost += unit->GetCircuitDef()->GetCost();
}

void CMilitaryManager::DelArmyCost(CCircuitUnit* unit)
{
	DelResponse(unit);
	armyCost = std::max(armyCost - unit->GetCircuitDef()->GetCost(), .0f);
}

/*
 * 2d only, ignores y component.
 * @see KAIK/AttackHandler::KMeansIteration for general reference
 */
void CMilitaryManager::KMeansIteration()
{
	const CCircuitAI::EnemyUnits& units = circuit->GetEnemyUnits();
	// calculate a new K. change the formula to adjust max K, needs to be 1 minimum.
	constexpr int KMEANS_BASE_MAX_K = 32;
	int newK = std::min(KMEANS_BASE_MAX_K, 1 + (int)sqrtf(units.size()));

	// change the number of means according to newK
	assert(newK > 0/* && enemyGoups.size() > 0*/);
	// add a new means, just use one of the positions
	AIFloat3 newMeansPosition = units.begin()->second->GetPos();
//	newMeansPosition.y = circuit->GetMap()->GetElevationAt(newMeansPosition.x, newMeansPosition.z) + K_MEANS_ELEVATION;
	enemyGroups.resize(newK, SEnemyGroup(newMeansPosition));

	// check all positions and assign them to means, complexity n*k for one iteration
	std::vector<int> unitsClosestMeanID(units.size(), -1);
	std::vector<int> numUnitsAssignedToMean(newK, 0);

	{
		int i = 0;
		for (const auto& kv : units) {
			CEnemyUnit* enemy = kv.second;
			if (enemy->IsHidden()) {
				continue;
			}
			AIFloat3 unitPos = enemy->GetPos();
			float closestDistance = std::numeric_limits<float>::max();
			int closestIndex = -1;

			for (int m = 0; m < newK; m++) {
				const AIFloat3& mean = enemyGroups[m].pos;
				float distance = unitPos.SqDistance2D(mean);

				if (distance < closestDistance) {
					closestDistance = distance;
					closestIndex = m;
				}
			}

			// position i is closest to the mean at closestIndex
			unitsClosestMeanID[i++] = closestIndex;
			numUnitsAssignedToMean[closestIndex]++;
		}
	}

	// change the means according to which positions are assigned to them
	// use meanAverage for indexes with 0 pos'es assigned
	// make a new means list
//	std::vector<AIFloat3> newMeans(newK, ZeroVector);
	std::vector<SEnemyGroup>& newMeans = enemyGroups;
	for (unsigned i = 0; i < newMeans.size(); i++) {
		SEnemyGroup& eg = newMeans[i];
		eg.units.clear();
		eg.units.reserve(numUnitsAssignedToMean[i]);
		eg.pos = ZeroVector;
		std::fill(eg.roleCosts.begin(), eg.roleCosts.end(), 0.f);
		eg.cost = 0.f;
		eg.threat = 0.f;
	}

	{
		int i = 0;
		for (const auto& kv : units) {
			CEnemyUnit* enemy = kv.second;
			if (enemy->IsHidden()) {
				continue;
			}
			int meanIndex = unitsClosestMeanID[i++];
			SEnemyGroup& eg = newMeans[meanIndex];

			// don't divide by 0
			float num = std::max(1, numUnitsAssignedToMean[meanIndex]);
			eg.pos += enemy->GetPos() / num;

			eg.units.push_back(kv.first);

			const CCircuitDef* cdef = enemy->GetCircuitDef();
			if (cdef != nullptr) {
				eg.roleCosts[cdef->GetMainRole()] += cdef->GetCost();
				if (!cdef->IsMobile() || enemy->IsInRadarOrLOS()) {
					eg.cost += cdef->GetCost();
				}
				eg.threat += enemy->GetThreat() * (cdef->IsMobile() ? initThrMod.inMobile : initThrMod.inStatic);
			} else {
				eg.threat += enemy->GetThreat();
			}
		}
	}

	// do a check and see if there are any empty means and set the height
	enemyPos = ZeroVector;
	for (int i = 0; i < newK; i++) {
		// if a newmean is unchanged, set it to the new means pos instead of (0, 0, 0)
		if (newMeans[i].pos == ZeroVector) {
			newMeans[i] = newMeansPosition;
		} else {
			// get the proper elevation for the y-coord
//			newMeans[i].pos.y = circuit->GetMap()->GetElevationAt(newMeans[i].pos.x, newMeans[i].pos.z) + K_MEANS_ELEVATION;
		}
		enemyPos += newMeans[i].pos;
	}
	enemyPos /= newK;

//	return newMeans;
}

} // namespace circuit
