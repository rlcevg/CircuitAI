/*
 * BuilderManager.cpp
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "resource/MetalManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "terrain/PathFinder.h"
#include "task/NullTask.h"
#include "task/IdleTask.h"
#include "task/RetreatTask.h"
#include "task/StuckTask.h"
#include "task/builder/FactoryTask.h"
#include "task/builder/NanoTask.h"
#include "task/builder/StoreTask.h"
#include "task/builder/PylonTask.h"
#include "task/builder/EnergyTask.h"
#include "task/builder/DefenceTask.h"
#include "task/builder/BunkerTask.h"
#include "task/builder/BigGunTask.h"
#include "task/builder/RadarTask.h"
#include "task/builder/MexTask.h"
#include "task/builder/TerraformTask.h"
#include "task/builder/RepairTask.h"
#include "task/builder/ReclaimTask.h"
#include "task/builder/PatrolTask.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "Command.h"

namespace circuit {

using namespace springai;

CBuilderManager::CBuilderManager(CCircuitAI* circuit) :
		IUnitModule(circuit),
		builderTasksCount(0),
		builderPower(.0f)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::Watchdog, this),
							FRAMES_PER_SEC * 60,
							circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 0);
	// Init after parallel clusterization
	scheduler->RunParallelTask(CGameTask::emptyTask,
							   std::make_shared<CGameTask>(&CBuilderManager::Init, this));

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

		builderPower += unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
		workers.insert(unit);

		AddBuildList(unit);
	};
	auto workerIdleHandler = [this](CCircuitUnit* unit) {
		// FIXME: Avoid instant task reassignment, its only valid on build order fail.
		//        Can cause idle builder, but Watchdog should catch it eventually.
		//        Unfortunately can't use EVENT_COMMAND_FINISHED because there is no unique commandId like unitId
		if (this->circuit->GetLastFrame() - unit->GetTaskFrame() > FRAMES_PER_SEC) {
			unit->GetTask()->OnUnitIdle(unit);
		}
	};
	auto workerDamagedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		unit->GetTask()->OnUnitDamaged(unit, attacker);
	};
	auto workerDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		unit->GetTask()->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		if (unit->GetTask() == nullTask) {  // alternative: unit->GetUnit()->IsBeingBuilt()
			return;
		}
		--buildAreas[unit->GetArea()][unit->GetCircuitDef()];

		builderPower -= unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
		workers.erase(unit);

		RemoveBuildList(unit);
	};

	/*
	 * building handlers
	 */
	auto buildingDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		Unit* u = unit->GetUnit();
		this->circuit->GetTerrainManager()->RemoveBlocker(unit->GetCircuitDef(), u->GetPos(), u->GetBuildingFacing());
	};

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	const CCircuitAI::CircuitDefs& defs = circuit->GetCircuitDefs();
	for (auto& kv : defs) {
		CCircuitDef* cdef = kv.second;
		if (cdef->IsMobile()) {
			if (cdef->GetUnitDef()->IsBuilder() && !cdef->GetBuildOptions().empty()) {
				CCircuitDef::Id unitDefId = kv.first;
				createdHandler[unitDefId] = workerCreatedHandler;
				finishedHandler[unitDefId] = workerFinishedHandler;
				idleHandler[unitDefId] = workerIdleHandler;
				damagedHandler[unitDefId] = workerDamagedHandler;
				destroyedHandler[unitDefId] = workerDestroyedHandler;

				int mtId = terrainManager->GetMobileTypeId(unitDefId);
				if (mtId >= 0) {  // not air
					workerMobileTypes.insert(mtId);
				}
				workerDefs.insert(cdef);
			}
		} else {
			destroyedHandler[kv.first] = buildingDestroyedHandler;
		}
	}

	/*
	 * cormex handlers; forbid from removing blocker
	 */
	CCircuitDef::Id unitDefId = circuit->GetEconomyManager()->GetMexDef()->GetId();
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		const AIFloat3& pos = unit->GetUnit()->GetPos();
		if (unit->GetUnit()->IsBeingBuilt() || !IsBuilderInArea(unit->GetCircuitDef(), pos)) {
			this->circuit->GetMetalManager()->SetOpenSpot(pos, true);
		} else {
			EnqueueTask(IBuilderTask::Priority::HIGH, unit->GetCircuitDef(), pos, IBuilderTask::BuildType::MEX)->SetBuildPos(pos);
		}
	};

	builderTasks.resize(static_cast<int>(IBuilderTask::BuildType::TASKS_COUNT));

	for (auto mtId : workerMobileTypes) {
		for (auto area : terrainManager->GetMobileTypeById(mtId)->area) {
			buildAreas[area] = std::map<CCircuitDef*, int>();
		}
	}
	buildAreas[nullptr] = std::map<CCircuitDef*, int>();  // air

	terraDef = circuit->GetCircuitDef("terraunit");

	// FIXME: EXPERIMENTAL
	/*
	 * strider handlers
	 */
//	const char* striders[] = {"armcomdgun", "scorpion", "dante", "armraven", "funnelweb", "armbanth", "armorco"};
//	for (auto strider : striders) {
//		createdHandler[circuit->GetCircuitDef(strider)->GetId()] = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
//			unfinishedUnits[unit] = EnqueueRepair(IBuilderTask::Priority::LOW, unit);
//		};
//	}
	// FIXME: EXPERIMENTAL
}

CBuilderManager::~CBuilderManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (auto& tasks : builderTasks) {
		utils::free_clear(tasks);
	}
	utils::free_clear(deleteTasks);
}

int CBuilderManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetCircuitDef()->GetId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	if (builder == nullptr) {
		return 0; //signaling: OK
	}

	IUnitTask* task = builder->GetTask();
	if (task->GetType() != IUnitTask::Type::BUILDER) {
		return 0; //signaling: OK
	}

	Unit* u = unit->GetUnit();
	IBuilderTask* taskB = static_cast<IBuilderTask*>(task);
	if (u->IsBeingBuilt()) {
		// FIXME: Try to cope with wrong event order, when different units created within same task.
		//        Real example: unit starts building, but hlt kills structure right away. UnitDestroyed invoked and new task assigned to unit.
		//        But for some engine-bugged reason unit is not idle and retries same building. UnitCreated invoked for new task with wrong target.
		//        Next workaround unfortunately doesn't mark bugged building on blocking map.
		// TODO: Create additional task to build/reclaim lost unit
		if ((taskB->GetTarget() == nullptr) && (taskB->GetBuildDef() != nullptr) &&
			(*taskB->GetBuildDef() == *unit->GetCircuitDef()) && taskB->IsEqualBuildPos(u->GetPos()))
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

	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

const std::set<IBuilderTask*>& CBuilderManager::GetTasks(IBuilderTask::BuildType type)
{
	// Auto-creates empty list
	return builderTasks[static_cast<int>(type)];
}

void CBuilderManager::ActivateTask(IBuilderTask* task)
{
	builderTasks[static_cast<int>(task->GetBuildType())].insert(task);
	builderTasksCount++;
}

IBuilderTask* CBuilderManager::EnqueueTask(IBuilderTask::Priority priority,
										   CCircuitDef* buildDef,
										   const AIFloat3& position,
										   IBuilderTask::BuildType type,
										   float cost,
										   bool isShake,
										   bool isActive,
										   int timeout)
{
	return AddTask(priority, buildDef, position, type, cost, isShake, isActive, timeout);
}

IBuilderTask* CBuilderManager::EnqueueTask(IBuilderTask::Priority priority,
										   CCircuitDef* buildDef,
										   const AIFloat3& position,
										   IBuilderTask::BuildType type,
										   bool isShake,
										   bool isActive,
										   int timeout)
{
	float cost = buildDef->GetCost();
	return AddTask(priority, buildDef, position, type, cost, isShake, isActive, timeout);
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
		builderTasks[static_cast<int>(IBuilderTask::BuildType::PYLON)].insert(task);
		builderTasksCount++;
	}
	return task;
}

IBuilderTask* CBuilderManager::EnqueuePatrol(IBuilderTask::Priority priority,
											 const AIFloat3& position,
											 float cost,
											 int timeout)
{
	IBuilderTask* task = new CBPatrolTask(this, priority, position, cost, timeout);
	builderTasks[static_cast<int>(IBuilderTask::BuildType::PATROL)].insert(task);
	builderTasksCount++;
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
	builderTasks[static_cast<int>(IBuilderTask::BuildType::RECLAIM)].insert(task);
	builderTasksCount++;
	return task;
}

IBuilderTask* CBuilderManager::EnqueueRepair(IBuilderTask::Priority priority,
											 CCircuitUnit* target,
											 int timeout)
{
	IBuilderTask* task = new CBRepairTask(this, priority, target, timeout);
	builderTasks[static_cast<int>(IBuilderTask::BuildType::REPAIR)].insert(task);
	builderTasksCount++;
	return task;
}

IBuilderTask* CBuilderManager::EnqueueTerraform(IBuilderTask::Priority priority,
												CCircuitUnit* target,
												float cost,
												int timeout)
{
	IBuilderTask* task = new CBTerraformTask(this, priority, target, cost, timeout);
	builderTasks[static_cast<int>(IBuilderTask::BuildType::TERRAFORM)].insert(task);
	builderTasksCount++;
	return task;
}

IBuilderTask* CBuilderManager::AddTask(IBuilderTask::Priority priority,
									   CCircuitDef* buildDef,
									   const AIFloat3& position,
									   IBuilderTask::BuildType type,
									   float cost,
									   bool isShake,
									   bool isActive,
									   int timeout)
{
	IBuilderTask* task;
	switch (type) {
		case IBuilderTask::BuildType::FACTORY: {
			task = new CBFactoryTask(this, priority, buildDef, position, cost, isShake, timeout);
			break;
		}
		case IBuilderTask::BuildType::NANO: {
			task = new CBNanoTask(this, priority, buildDef, position, cost, isShake, timeout);
			break;
		}
		case IBuilderTask::BuildType::STORE: {
			task = new CBStoreTask(this, priority, buildDef, position, cost, isShake, timeout);
			break;
		}
		case IBuilderTask::BuildType::ENERGY: {
			task = new CBEnergyTask(this, priority, buildDef, position, cost, isShake, timeout);
			break;
		}
		case IBuilderTask::BuildType::DEFENCE: {
			task = new CBDefenceTask(this, priority, buildDef, position, cost, isShake, timeout);
			break;
		}
		case IBuilderTask::BuildType::BUNKER: {
			task = new CBBunkerTask(this, priority, buildDef, position, cost, isShake, timeout);
			break;
		}
		default:
		case IBuilderTask::BuildType::BIG_GUN: {
			task = new CBBigGunTask(this, priority, buildDef, position, cost, isShake, timeout);
			break;
		}
		case IBuilderTask::BuildType::RADAR: {
			task = new CBRadarTask(this, priority, buildDef, position, cost, isShake, timeout);
			break;
		}
		case IBuilderTask::BuildType::MEX: {
			task = new CBMexTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
	}

	if (isActive) {
		builderTasks[static_cast<int>(type)].insert(task);
		builderTasksCount++;
	}
	// TODO: Send NewTask message
	return task;
}

void CBuilderManager::DequeueTask(IBuilderTask* task, bool done)
{
	std::set<IBuilderTask*>& tasks = builderTasks[static_cast<int>(task->GetBuildType())];
	auto it = tasks.find(task);
	if (it != tasks.end()) {
		unfinishedUnits.erase(task->GetTarget());
		tasks.erase(it);
		task->Close(done);
		deleteTasks.insert(task);
		builderTasksCount--;
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

void CBuilderManager::AssignTask(CCircuitUnit* unit)
{
	IBuilderTask* task = nullptr;
	AIFloat3 pos = unit->GetUnit()->GetPos();

	circuit->GetThreatMap()->SetThreatType(unit);
	task = circuit->GetEconomyManager()->UpdateMetalTasks(pos, unit);
	if (task != nullptr) {
		task->AssignTo(unit);
		return;
	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	terrainManager->CorrectPosition(pos);
	circuit->GetPathfinder()->SetMapData(unit, circuit->GetThreatMap());
	float maxSpeed = unit->GetUnit()->GetMaxSpeed();
	int buildDistance = unit->GetCircuitDef()->GetBuildDistance();
	float metric = std::numeric_limits<float>::max();
	for (auto& tasks : builderTasks) {
		for (auto candidate : tasks) {
			if (!candidate->CanAssignTo(unit)) {
				continue;
			}

			// Check time-distance to target
			float weight = (static_cast<float>(candidate->GetPriority()) + 1.0f);
			weight = 1.0f / (weight * weight);
			float dist;
			bool valid = false;

			CCircuitUnit* target = candidate->GetTarget();
			if (target != nullptr) {
				AIFloat3 buildPos = candidate->GetBuildPos();

				if (!terrainManager->CanBuildAt(unit, buildPos)) {
					continue;
				}

				Unit* tu = target->GetUnit();
				dist = circuit->GetPathfinder()->PathCost(pos, buildPos, buildDistance);
				if (dist * weight < metric) {
					float maxHealth = tu->GetMaxHealth();
					float healthSpeed = maxHealth * candidate->GetBuildPower() / candidate->GetCost();
					valid = (((maxHealth - tu->GetHealth()) * 0.6f) > healthSpeed * (dist / (maxSpeed * FRAMES_PER_SEC)));
				}

			} else {

				const AIFloat3& bp = candidate->GetPosition();
				AIFloat3 buildPos = (bp != -RgtVector) ? bp : pos;

				if (!terrainManager->CanBuildAt(unit, buildPos)) {
					continue;
				}

				dist = circuit->GetPathfinder()->PathCost(pos, buildPos, buildDistance);
				valid = ((dist * weight < metric) && (dist / (maxSpeed * FRAMES_PER_SEC) < MAX_TRAVEL_SEC));
			}

			if (valid) {
				task = candidate;
				metric = dist * weight;
			}
		}
	}

	if (task == nullptr) {
		task = CreateBuilderTask(pos, unit);
	}

	task->AssignTo(unit);
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

	IBuilderTask* task = EnqueuePatrol(IBuilderTask::Priority::LOW, unit->GetUnit()->GetPos(), .0f, FRAMES_PER_SEC * 20);
	task->AssignTo(unit);
	task->Execute(unit);
}

void CBuilderManager::Init()
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	const int interval = 8;
	const int offset = circuit->GetSkirmishAIId() % interval;
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::UpdateIdle, this), interval, offset + 0);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::UpdateRetreat, this), interval, offset + 1);
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
	} else if (metalIncome < 100) {
		task = EnqueuePatrol(IBuilderTask::Priority::LOW, position, .0f, FRAMES_PER_SEC * 20);
	} else {
		const std::set<IBuilderTask*>& tasks = GetTasks(IBuilderTask::BuildType::BIG_GUN);
		if (tasks.empty()) {
			buildDef = circuit->GetCircuitDef("raveparty");
			if ((buildDef->GetCount() < 1) && buildDef->IsAvailable()) {
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
			static_cast<ITaskManager*>(this)->AssignTask(worker, new CStuckTask(this));
		}
		utils::free_clear(commands);
	}

	// find unfinished abandoned buildings
	float maxCost = MAX_BUILD_SEC * std::min(economyManager->GetAvgMetalIncome(), builderPower) * economyManager->GetEcoFactor();
	// TODO: Include special units
	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;
		Unit* u = unit->GetUnit();
		if ((unfinishedUnits.find(unit) == unfinishedUnits.end()) && (u->GetMaxSpeed() <= 0)) {
			if (u->IsBeingBuilt()) {
				float maxHealth = u->GetMaxHealth();
				float buildPercent = (maxHealth - u->GetHealth()) / maxHealth;
				CCircuitDef* cdef = unit->GetCircuitDef();
				if ((cdef->GetCost() * buildPercent < maxCost) || (*cdef == *terraDef)) {
					unfinishedUnits[unit] = EnqueueRepair(IBuilderTask::Priority::NORMAL, unit);
				}
			} else if (u->GetHealth() < u->GetMaxHealth()) {
				unfinishedUnits[unit] = EnqueueRepair(IBuilderTask::Priority::NORMAL, unit);
			}
		}
	}
}

void CBuilderManager::UpdateIdle()
{
	idleTask->Update();
}

void CBuilderManager::UpdateRetreat()
{
	retreatTask->Update();
}

void CBuilderManager::UpdateBuild()
{
	if (!deleteTasks.empty()) {
		for (IBuilderTask* task : deleteTasks) {
			updateTasks.erase(task);
			delete task;
		}
		deleteTasks.clear();
	}

	auto it = updateTasks.begin();
	unsigned int i = 0;
	int lastFrame = circuit->GetLastFrame();
	while (it != updateTasks.end()) {
		IBuilderTask* task = *it;

		int frame = task->GetLastTouched();
		int timeout = task->GetTimeout();
		if ((frame != -1) && (timeout > 0) && (lastFrame - frame >= timeout)) {
			AbortTask(task);
		} else {
			task->Update();
		}

		it = updateTasks.erase(it);
		if (++i >= updateSlice) {
			break;
		}
	}

	if (updateTasks.empty()) {
		for (auto& tasks : builderTasks) {
			updateTasks.insert(tasks.begin(), tasks.end());
		}
		updateSlice = updateTasks.size() / TEAM_SLOWUPDATE_RATE;
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
	for (auto& tasks : builderTasks) {
		for (auto task : tasks) {
			CCircuitDef* cdef = task->GetBuildDef();
			if ((cdef != nullptr) && !IsBuilderInArea(cdef, task->GetPosition())) {
				removeTasks.insert(task);
			}
		}
	}
	for (auto task : removeTasks) {
		AbortTask(task);
	}
}

} // namespace circuit
