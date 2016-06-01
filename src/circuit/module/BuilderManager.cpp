/*
 * BuilderManager.cpp
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "terrain/PathFinder.h"
#include "task/NullTask.h"
#include "task/IdleTask.h"
#include "task/RetreatTask.h"
#include "task/builder/FactoryTask.h"
#include "task/builder/NanoTask.h"
#include "task/builder/StoreTask.h"
#include "task/builder/PylonTask.h"
#include "task/builder/EnergyTask.h"
#include "task/builder/DefenceTask.h"
#include "task/builder/BunkerTask.h"
#include "task/builder/BigGunTask.h"
#include "task/builder/RadarTask.h"
#include "task/builder/SonarTask.h"
#include "task/builder/MexTask.h"
#include "task/builder/TerraformTask.h"
#include "task/builder/RepairTask.h"
#include "task/builder/ReclaimTask.h"
#include "task/builder/PatrolTask.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#include "json/json.h"

#include "Command.h"

namespace circuit {

using namespace springai;

CBuilderManager::CBuilderManager(CCircuitAI* circuit)
		: IUnitModule(circuit)
		, buildTasksCount(0)
		, builderPower(.0f)
		, buildUpdateSlice(0)
		, miscUpdateSlice(0)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::Watchdog, this),
							FRAMES_PER_SEC * 60,
							circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 10);
	scheduler->RunTaskAt(std::make_shared<CGameTask>(&CBuilderManager::Init, this));

	/*
	 * worker handlers
	 */
	auto workerCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nullTask->AssignTo(unit);
		}
	};
	auto workerFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			idleTask->AssignTo(unit);
		} else {
			nullTask->RemoveAssignee(unit);
		}

		++buildAreas[unit->GetArea()][unit->GetCircuitDef()];

		builderPower += unit->GetCircuitDef()->GetBuildSpeed();
		workers.insert(unit);

		AddBuildList(unit);
	};
	auto workerIdleHandler = [this](CCircuitUnit* unit) {
		// FIXME: Avoid instant task reassignment, its only valid on build order fail.
		//        Can cause idle builder, but Watchdog should catch it eventually.
		//        Unfortunately can't use EVENT_COMMAND_FINISHED because there is no unique commandId like unitId
		if (this->circuit->GetLastFrame() > unit->GetTaskFrame()/* + FRAMES_PER_SEC*/) {
			unit->GetTask()->OnUnitIdle(unit);
		}
	};
	auto workerDamagedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		unit->GetTask()->OnUnitDamaged(unit, attacker);
	};
	auto workerDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		if (unit->GetUnit()->IsBeingBuilt()) {  // alternative: task == nullTask
			return;
		}
		--buildAreas[unit->GetArea()][unit->GetCircuitDef()];

		builderPower -= unit->GetCircuitDef()->GetBuildSpeed();
		workers.erase(unit);

		RemoveBuildList(unit);
	};

	/*
	 * building handlers
	 */
	auto buildingDamagedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		EnqueueRepair(IBuilderTask::Priority::NORMAL, unit);
	};
	auto buildingDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		int frame = this->circuit->GetLastFrame();
		int facing = unit->GetUnit()->GetBuildingFacing();
		this->circuit->GetTerrainManager()->DelBlocker(unit->GetCircuitDef(), unit->GetPos(frame), facing);
	};

	/*
	 * heavy handlers
	 */
	auto heavyCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		CEconomyManager* economyManager = this->circuit->GetEconomyManager();
		if (economyManager->GetAvgMetalIncome() * economyManager->GetEcoFactor() > 16.0f) {
			unit->GetUnit()->ExecuteCustomCommand(CMD_PRIORITY, {2.0f});
//			EnqueueRepair(IBuilderTask::Priority::LOW, unit);
		}
	};

	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const Json::Value& retreats = root["retreat"];
	const float builderRet = retreats.get("_builder_", 0.8f).asFloat();

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	const CCircuitAI::CircuitDefs& allDefs = circuit->GetCircuitDefs();
	for (auto& kv : allDefs) {
		CCircuitDef::Id unitDefId = kv.first;
		CCircuitDef* cdef = kv.second;
		if (cdef->IsMobile()) {
			if (cdef->GetUnitDef()->IsBuilder() && !cdef->GetBuildOptions().empty()) {
				createdHandler[unitDefId]   = workerCreatedHandler;
				finishedHandler[unitDefId]  = workerFinishedHandler;
				idleHandler[unitDefId]      = workerIdleHandler;
				damagedHandler[unitDefId]   = workerDamagedHandler;
				destroyedHandler[unitDefId] = workerDestroyedHandler;

				int mtId = terrainManager->GetMobileTypeId(unitDefId);
				if (mtId >= 0) {  // not air
					workerMobileTypes.insert(mtId);
				}
				workerDefs.insert(cdef);

				const char* name = cdef->GetUnitDef()->GetName();
				cdef->SetRetreat(retreats.get(name, builderRet).asFloat());
			} else if (cdef->GetCost() > 999.0f) {
				createdHandler[unitDefId] = heavyCreatedHandler;
			}
		} else {
			damagedHandler[unitDefId]   = buildingDamagedHandler;
			destroyedHandler[unitDefId] = buildingDestroyedHandler;
		}
	}

	/*
	 * cormex handlers; forbid from removing blocker
	 */
	CCircuitDef::Id unitDefId = circuit->GetEconomyManager()->GetMexDef()->GetId();
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		const AIFloat3& pos = unit->GetPos(this->circuit->GetLastFrame());
		CCircuitDef* mexDef = unit->GetCircuitDef();
		int index = this->circuit->GetMetalManager()->FindNearestSpot(pos);
		if (index < 0) {
			return;
		}
		this->circuit->GetMetalManager()->SetOpenSpot(index, true);
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		// Check mex position in 20 seconds
		this->circuit->GetScheduler()->RunTaskAfter(std::make_shared<CGameTask>([this, mexDef, pos, index]() {
			if (this->circuit->GetMetalManager()->IsOpenSpot(index) &&
				this->circuit->GetBuilderManager()->IsBuilderInArea(mexDef, pos) &&
				this->circuit->GetTerrainManager()->CanBeBuiltAt(mexDef, pos))
			{
				EnqueueTask(IBuilderTask::Priority::HIGH, mexDef, pos, IBuilderTask::BuildType::MEX)->SetBuildPos(pos);
				this->circuit->GetMetalManager()->SetOpenSpot(index, false);
			}
		}), FRAMES_PER_SEC * 20);
	};

	buildTasks.resize(static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::TASKS_COUNT));

	for (auto mtId : workerMobileTypes) {
		for (auto area : terrainManager->GetMobileTypeById(mtId)->area) {
			buildAreas[area] = std::map<CCircuitDef*, int>();
		}
	}
	buildAreas[nullptr] = std::map<CCircuitDef*, int>();  // air

	terraDef = circuit->GetCircuitDef("terraunit");  // TODO: Move into config
}

CBuilderManager::~CBuilderManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (std::set<IBuilderTask*>& tasks : buildTasks) {
		utils::free_clear(tasks);
	}
	utils::free_clear(buildDeleteTasks);

	utils::free_clear(miscTasks);
	utils::free_clear(miscDeleteTasks);
}

int CBuilderManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetCircuitDef()->GetId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	if (builder == nullptr) {
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		if (!unit->GetCircuitDef()->IsMobile() && !terrainManager->ResignAllyBuilding(unit)) {
			// enemy unit captured
			const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
			terrainManager->AddBlocker(unit->GetCircuitDef(), pos, unit->GetUnit()->GetBuildingFacing());
		}
		return 0; //signaling: OK
	}

	IUnitTask* task = builder->GetTask();
	if (task->GetType() != IUnitTask::Type::BUILDER) {
		return 0; //signaling: OK
	}

	IBuilderTask* taskB = static_cast<IBuilderTask*>(task);
	if (unit->GetUnit()->IsBeingBuilt()) {
		// NOTE: Try to cope with wrong event order, when different units created within same task.
		//       Real example: unit starts building, but hlt kills structure right away. UnitDestroyed invoked and new task assigned to unit.
		//       But for some engine-bugged reason unit is not idle and retries same building. UnitCreated invoked for new task with wrong target.
		//       Next workaround unfortunately doesn't mark bugged building on blocking map.
		// TODO: Create additional task to build/reclaim lost unit
		if ((taskB->GetTarget() == nullptr) && (taskB->GetBuildDef() != nullptr) &&
			(*taskB->GetBuildDef() == *unit->GetCircuitDef()) && taskB->IsEqualBuildPos(unit))
		{
			taskB->UpdateTarget(unit);
			unfinishedUnits[unit] = taskB;
		}
	} else {
		DoneTask(taskB);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitFinished(CCircuitUnit* unit)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		DoneTask(iter->second);
	}
	auto itre = repairedUnits.find(unit->GetId());
	if (itre != repairedUnits.end()) {
		DoneTask(itre->second);
	}

	auto search = finishedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetCircuitDef()->GetId());
	if (search != idleHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	auto search = damagedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != damagedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		AbortTask(iter->second);
	}
	auto itre = repairedUnits.find(unit->GetId());
	if (itre != repairedUnits.end()) {
		AbortTask(itre->second);
	}

	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

const std::set<IBuilderTask*>& CBuilderManager::GetTasks(IBuilderTask::BuildType type) const
{
	assert(type < IBuilderTask::BuildType::TASKS_COUNT);
	return buildTasks[static_cast<IBuilderTask::BT>(type)];
}

void CBuilderManager::ActivateTask(IBuilderTask* task)
{
	if ((task->GetType() == IUnitTask::Type::BUILDER) && (task->GetBuildType() < IBuilderTask::BuildType::TASKS_COUNT)) {
		buildTasks[static_cast<IBuilderTask::BT>(task->GetBuildType())].insert(task);
		buildTasksCount++;
	} else {
		miscTasks.insert(task);
	}
}

IBuilderTask* CBuilderManager::EnqueueTask(IBuilderTask::Priority priority,
										   CCircuitDef* buildDef,
										   const AIFloat3& position,
										   IBuilderTask::BuildType type,
										   float cost,
										   float shake,
										   bool isActive,
										   int timeout)
{
	return AddTask(priority, buildDef, position, type, cost, shake, isActive, timeout);
}

IBuilderTask* CBuilderManager::EnqueueTask(IBuilderTask::Priority priority,
										   CCircuitDef* buildDef,
										   const AIFloat3& position,
										   IBuilderTask::BuildType type,
										   float shake,
										   bool isActive,
										   int timeout)
{
	float cost = buildDef->GetCost();
	return AddTask(priority, buildDef, position, type, cost, shake, isActive, timeout);
}

IBuilderTask* CBuilderManager::EnqueuePylon(IBuilderTask::Priority priority,
											CCircuitDef* buildDef,
											const AIFloat3& position,
											CEnergyLink* link,
											float cost,
											bool isActive,
											int timeout)
{
	IBuilderTask* task = new CBPylonTask(this, priority, buildDef, position, link, cost, timeout);
	if (isActive) {
		buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::PYLON)].insert(task);
		buildTasksCount++;
	}
	return task;
}

IBuilderTask* CBuilderManager::EnqueueRepair(IBuilderTask::Priority priority,
											 CCircuitUnit* target,
											 int timeout)
{
	auto it = repairedUnits.find(target->GetId());
	if (it != repairedUnits.end()) {
		return it->second;
	}
	CBRepairTask* task = new CBRepairTask(this, priority, target, timeout);
	buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::REPAIR)].insert(task);
	buildTasksCount++;
	repairedUnits[target->GetId()] = task;
	return task;
}

IBuilderTask* CBuilderManager::EnqueueReclaim(IBuilderTask::Priority priority,
											  const AIFloat3& position,
											  float cost,
											  int timeout,
											  float radius,
											  bool isMetal)
{
	IBuilderTask* task = new CBReclaimTask(this, priority, position, cost, timeout, radius, isMetal);
	buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::RECLAIM)].insert(task);
	buildTasksCount++;
	return task;
}

IBuilderTask* CBuilderManager::EnqueuePatrol(IBuilderTask::Priority priority,
											 const AIFloat3& position,
											 float cost,
											 int timeout)
{
	IBuilderTask* task = new CBPatrolTask(this, priority, position, cost, timeout);
	miscTasks.insert(task);
	return task;
}

IBuilderTask* CBuilderManager::EnqueueTerraform(IBuilderTask::Priority priority,
												CCircuitUnit* target,
												const AIFloat3& position,
												float cost,
												bool isActive,
												int timeout)
{
	IBuilderTask* task;
	if (target == nullptr) {
		task = new CBTerraformTask(this, priority, position, cost, timeout);
	} else {
		task = new CBTerraformTask(this, priority, target, cost, timeout);
	}
	if (isActive) {
		miscTasks.insert(task);
	}
	return task;
}

CRetreatTask* CBuilderManager::EnqueueRetreat()
{
	CRetreatTask* task = new CRetreatTask(this);
	miscTasks.insert(task);
	return task;
}

IBuilderTask* CBuilderManager::AddTask(IBuilderTask::Priority priority,
									   CCircuitDef* buildDef,
									   const AIFloat3& position,
									   IBuilderTask::BuildType type,
									   float cost,
									   float shake,
									   bool isActive,
									   int timeout)
{
	IBuilderTask* task;
	switch (type) {
		case IBuilderTask::BuildType::FACTORY: {
			task = new CBFactoryTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::NANO: {
			task = new CBNanoTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::STORE: {
			task = new CBStoreTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::ENERGY: {
			task = new CBEnergyTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::DEFENCE: {
			task = new CBDefenceTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::BUNKER: {
			task = new CBBunkerTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		default:
		case IBuilderTask::BuildType::BIG_GUN: {
			task = new CBBigGunTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::RADAR: {
			task = new CBRadarTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::SONAR: {
			task = new CBSonarTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::MEX: {
			task = new CBMexTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
	}

	if (isActive) {
		buildTasks[static_cast<IBuilderTask::BT>(type)].insert(task);
		buildTasksCount++;
	}
	return task;
}

void CBuilderManager::DequeueTask(IBuilderTask* task, bool done)
{
	if ((task->GetType() == IUnitTask::Type::BUILDER) && (task->GetBuildType() < IBuilderTask::BuildType::TASKS_COUNT)) {
		std::set<IBuilderTask*>& tasks = buildTasks[static_cast<IBuilderTask::BT>(task->GetBuildType())];
		auto it = tasks.find(task);
		if (it != tasks.end()) {
			if (task->GetBuildType() == IBuilderTask::BuildType::REPAIR) {
				repairedUnits.erase(static_cast<CBRepairTask*>(task)->GetTargetId());
			} else {
				unfinishedUnits.erase(task->GetTarget());
			}
			tasks.erase(it);
			task->Close(done);
			buildDeleteTasks.insert(task);
			buildTasksCount--;
		}
	} else {
		auto it = miscTasks.find(task);
		if (it != miscTasks.end()) {
			miscTasks.erase(task);
			task->Close(done);
			miscDeleteTasks.insert(task);
		}
	}
}

bool CBuilderManager::IsBuilderInArea(CCircuitDef* buildDef, const AIFloat3& position)
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	for (auto& kv : buildAreas) {
		for (auto& kvw : kv.second) {
			if ((kvw.second > 0) && kvw.first->CanBuild(buildDef) &&
				terrainManager->CanMobileBuildAt(kv.first, kvw.first, position))
			{
				return true;
			}
		}
	}
	return false;
}

IUnitTask* CBuilderManager::MakeTask(CCircuitUnit* unit)
{
	circuit->GetThreatMap()->SetThreatType(unit);
	const IBuilderTask* task = nullptr;
	int frame = circuit->GetLastFrame();
	AIFloat3 pos = unit->GetPos(frame);

	task = circuit->GetEconomyManager()->UpdateMetalTasks(pos, unit);
//	if (task != nullptr) {
//		return task;
//	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CPathFinder* pathfinder = circuit->GetPathfinder();
	terrainManager->CorrectPosition(pos);
	pathfinder->SetMapData(unit, circuit->GetThreatMap(), frame);
	const float maxSpeed = unit->GetUnit()->GetMaxSpeed() / pathfinder->GetSquareSize() * THREAT_BASE;
	const int buildDistance = std::max<int>(unit->GetCircuitDef()->GetBuildDistance(), pathfinder->GetSquareSize());
	float metric = std::numeric_limits<float>::max();
	for (const std::set<IBuilderTask*>& tasks : buildTasks) {
		for (const IBuilderTask* candidate : tasks) {
			if (!candidate->CanAssignTo(unit)) {
				continue;
			}

			// Check time-distance to target
			float weight = (static_cast<float>(candidate->GetPriority()) + 1.0f);
			weight = 1.0f / (weight * weight);
			float distCost;
			bool valid = false;

			CCircuitUnit* target = candidate->GetTarget();
			if (target != nullptr) {
				AIFloat3 buildPos = candidate->GetBuildPos();

				if (!terrainManager->CanBuildAt(unit, buildPos)) {  // ensure that path always exists
					continue;
				}

				distCost = pathfinder->PathCost(pos, buildPos, buildDistance);
				if (distCost > pathfinder->PathCostDirect(pos, buildPos, buildDistance) * 1.05f) {
					continue;
				}
				distCost = std::max(distCost, THREAT_BASE);

				if (distCost * weight < metric) {
					Unit* tu = target->GetUnit();
					const float maxHealth = tu->GetMaxHealth();
					const float health = tu->GetHealth() - maxHealth * 0.005f;
					const float healthSpeed = maxHealth * candidate->GetBuildPower() / candidate->GetCost();
					valid = (((maxHealth - health) * 0.6f) * (maxSpeed * FRAMES_PER_SEC) > healthSpeed * distCost);
				}

			} else {

				const AIFloat3& bp = candidate->GetPosition();
				AIFloat3 buildPos = utils::is_valid(bp) ? bp : pos;

				if (!terrainManager->CanBuildAt(unit, buildPos)) {  // ensure that path always exists
					continue;
				}

				distCost = pathfinder->PathCost(pos, buildPos, buildDistance);
				if (distCost > pathfinder->PathCostDirect(pos, buildPos, buildDistance) * 1.05f) {
					continue;
				}
				distCost = std::max(distCost, THREAT_BASE);

				valid = ((distCost * weight < metric) && (distCost < MAX_TRAVEL_SEC * (maxSpeed * FRAMES_PER_SEC)));
			}

			if (valid) {
				task = candidate;
				metric = distCost * weight;
			}
		}
	}

	if (task == nullptr) {
		if (unit->GetTask() != idleTask) {
			return nullptr;  // current task is in danger or unreachable
		}
		task = CreateBuilderTask(pos, unit);
	}

	return const_cast<IBuilderTask*>(task);
}

void CBuilderManager::AbortTask(IUnitTask* task)
{
	// NOTE: Don't send Stop command, save some traffic.
	DequeueTask(static_cast<IBuilderTask*>(task), false);
}

void CBuilderManager::DoneTask(IUnitTask* task)
{
	DequeueTask(static_cast<IBuilderTask*>(task), true);
}

void CBuilderManager::FallbackTask(CCircuitUnit* unit)
{
	DequeueTask(static_cast<IBuilderTask*>(unit->GetTask()));

	int frame = circuit->GetLastFrame();
	IBuilderTask* task = EnqueuePatrol(IBuilderTask::Priority::LOW, unit->GetPos(frame), .0f, FRAMES_PER_SEC * 20);
	task->AssignTo(unit);
	task->Execute(unit);
}

void CBuilderManager::Init()
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	const int interval = 8;
	const int offset = circuit->GetSkirmishAIId() % interval;
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::UpdateIdle, this), interval, offset + 0);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::UpdateMisc, this), interval, offset + 1);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::UpdateBuild, this), interval, offset + 2);
}

IBuilderTask* CBuilderManager::CreateBuilderTask(const AIFloat3& position, CCircuitUnit* unit)
{
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	IBuilderTask* task;
	task = economyManager->UpdateEnergyTasks(position, unit);
	if (task != nullptr) {
		return task;
	}

	// FIXME: Eco rules. It should never get here
	float metalIncome = std::min(economyManager->GetAvgMetalIncome(), economyManager->GetAvgEnergyIncome()) * economyManager->GetEcoFactor();
	CCircuitDef* buildDef = circuit->GetCircuitDef("armwin");
	if ((metalIncome < 50) && (buildDef->GetCount() < 10) && buildDef->IsAvailable()) {
		task = EnqueueTask(IBuilderTask::Priority::NORMAL, buildDef, position, IBuilderTask::BuildType::ENERGY);
	} else if (metalIncome < 120) {  // TODO: Calc income of the map
		task = EnqueuePatrol(IBuilderTask::Priority::LOW, position, .0f, FRAMES_PER_SEC * 20);
	} else {
		const std::set<IBuilderTask*>& tasks = GetTasks(IBuilderTask::BuildType::BIG_GUN);
		if (tasks.empty()) {
			buildDef = circuit->GetCircuitDef("raveparty");
			if (circuit->GetMilitaryManager()->IsNeedBigGun(buildDef) && (buildDef->GetCount() < 1) && buildDef->IsAvailable()) {
				task = EnqueueTask(IBuilderTask::Priority::NORMAL, buildDef, circuit->GetSetupManager()->GetBasePos(), IBuilderTask::BuildType::BIG_GUN);
			} else {
				task = EnqueuePatrol(IBuilderTask::Priority::LOW, position, .0f, FRAMES_PER_SEC * 20);
			}
		} else {
			task = *tasks.begin();
		}
	}

	assert(task != nullptr);
	return task;
}

void CBuilderManager::AddBuildList(CCircuitUnit* unit)
{
	CCircuitDef* cDef = unit->GetCircuitDef();
	if (cDef->GetCount() > 1) {
		return;
	}

	const std::unordered_set<CCircuitDef::Id>& buildOptions = cDef->GetBuildOptions();
	std::set<CCircuitDef*> buildDefs;
	for (CCircuitDef::Id build : buildOptions) {
		CCircuitDef* cdef = circuit->GetCircuitDef(build);
		if (cdef->GetBuildCount() == 0) {
			buildDefs.insert(cdef);
		}
		cdef->IncBuild();
	}

	if (!buildDefs.empty()) {
		circuit->GetEconomyManager()->AddEnergyDefs(buildDefs);
	}

	// TODO: Same thing with factory, etc.
}

void CBuilderManager::RemoveBuildList(CCircuitUnit* unit)
{
	CCircuitDef* cDef = unit->GetCircuitDef();
	if (cDef->GetCount() > 1) {
		return;
	}

	const std::unordered_set<CCircuitDef::Id>& buildOptions = cDef->GetBuildOptions();
	std::set<CCircuitDef*> buildDefs;
	for (CCircuitDef::Id build : buildOptions) {
		CCircuitDef* cdef = circuit->GetCircuitDef(build);
		cdef->DecBuild();
		if (cdef->GetBuildCount() == 0) {
			buildDefs.insert(cdef);
		}
	}

	if (!buildDefs.empty()) {  // throws exception on set::erase otherwise
		circuit->GetEconomyManager()->RemoveEnergyDefs(buildDefs);
	}

	// TODO: Same thing with factory, etc.
}

void CBuilderManager::Watchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	Resource* metalRes = economyManager->GetMetalRes();
	// somehow workers get stuck
	for (CCircuitUnit* worker : workers) {
		Unit* u = worker->GetUnit();
		auto commands = std::move(u->GetCurrentCommands());
		// TODO: Ignore workers with idle and wait task? (.. && worker->GetTask()->IsBusy())
		if (commands.empty() && (u->GetResourceUse(metalRes) == .0f) && (u->GetVel() == ZeroVector)) {
			worker->GetTask()->OnUnitMoveFailed(worker);
		}
		utils::free_clear(commands);
	}

	// find unfinished abandoned buildings
	float maxCost = MAX_BUILD_SEC * std::min(economyManager->GetAvgMetalIncome(), builderPower) * economyManager->GetEcoFactor();
	// TODO: Include special units
	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;
		if (!unit->GetCircuitDef()->IsMobile() && (unfinishedUnits.find(unit) == unfinishedUnits.end())) {
			Unit* u = unit->GetUnit();
			if (u->IsBeingBuilt()) {
				float maxHealth = u->GetMaxHealth();
				float buildPercent = (maxHealth - u->GetHealth()) / maxHealth;
				CCircuitDef* cdef = unit->GetCircuitDef();
				if ((cdef->GetCost() * buildPercent < maxCost) || (*cdef == *terraDef)) {
					EnqueueRepair(IBuilderTask::Priority::NORMAL, unit);
				}
//			} else if (u->GetHealth() < u->GetMaxHealth()) {
//				EnqueueRepair(IBuilderTask::Priority::NORMAL, unit);
			}
		}
	}
}

void CBuilderManager::UpdateIdle()
{
	idleTask->Update();
}

void CBuilderManager::UpdateMisc()
{
	if (!miscDeleteTasks.empty()) {
		for (IUnitTask* task : miscDeleteTasks) {
			miscUpdateTasks.erase(task);
			delete task;
		}
		miscDeleteTasks.clear();
	}

	auto it = miscUpdateTasks.begin();
	unsigned int i = 0;
	int lastFrame = circuit->GetLastFrame();
	while (it != miscUpdateTasks.end()) {
		IUnitTask* task = *it;

		int frame = task->GetLastTouched();
		int timeout = task->GetTimeout();
		if ((frame != -1) && (timeout > 0) && (lastFrame - frame >= timeout)) {
			AbortTask(task);
		} else {
			task->Update();
		}

		it = miscUpdateTasks.erase(it);
		if (++i >= miscUpdateSlice) {
			break;
		}
	}

	if (miscUpdateTasks.empty()) {
		miscUpdateTasks.insert(miscTasks.begin(), miscTasks.end());
		miscUpdateSlice = miscUpdateTasks.size() / TEAM_SLOWUPDATE_RATE;
	}
}

void CBuilderManager::UpdateBuild()
{
	if (!buildDeleteTasks.empty()) {
		for (IBuilderTask* task : buildDeleteTasks) {
			buildUpdateTasks.erase(task);
			delete task;
		}
		buildDeleteTasks.clear();
	}

	auto it = buildUpdateTasks.begin();
	unsigned int i = 0;
	int lastFrame = circuit->GetLastFrame();
	while (it != buildUpdateTasks.end()) {
		IBuilderTask* task = *it;

		int frame = task->GetLastTouched();
		int timeout = task->GetTimeout();
		if ((frame != -1) && (timeout > 0) && (lastFrame - frame >= timeout)) {
			AbortTask(task);
		} else {
			task->Update();
		}

		it = buildUpdateTasks.erase(it);
		if (++i >= buildUpdateSlice) {
			break;
		}
	}

	if (buildUpdateTasks.empty()) {
		for (auto& tasks : buildTasks) {
			buildUpdateTasks.insert(tasks.begin(), tasks.end());
		}
		buildUpdateSlice = buildUpdateTasks.size() / TEAM_SLOWUPDATE_RATE;
	}
}

void CBuilderManager::UpdateAreaUsers()
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	buildAreas.clear();
	for (auto mtId : workerMobileTypes) {
		for (auto area : terrainManager->GetMobileTypeById(mtId)->area) {
			buildAreas[area] = std::map<CCircuitDef*, int>();
		}
	}
	buildAreas[nullptr] = std::map<CCircuitDef*, int>();  // air

	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;
		if (workerDefs.find(unit->GetCircuitDef()) != workerDefs.end()) {
			++buildAreas[unit->GetArea()][unit->GetCircuitDef()];
		}
	}

	std::set<IBuilderTask*> removeTasks;
	for (auto& tasks : buildTasks) {
		for (IBuilderTask* task : tasks) {
			CCircuitDef* cdef = task->GetBuildDef();
			if ((cdef != nullptr) && !IsBuilderInArea(cdef, task->GetPosition())) {
				removeTasks.insert(task);
			}
		}
	}
	for (IBuilderTask* task : removeTasks) {
		AbortTask(task);
	}
}

} // namespace circuit
