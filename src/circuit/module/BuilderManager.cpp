/*
 * BuilderManager.cpp
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "static/SetupManager.h"
#include "static/MetalManager.h"
#include "unit/CircuitUnit.h"
#include "terrain/TerrainManager.h"
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
#include "task/builder/MexTask.h"
#include "task/builder/TerraformTask.h"
#include "task/builder/RepairTask.h"
#include "task/builder/ReclaimTask.h"
#include "task/builder/PatrolTask.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "UnitDef.h"
#include "Unit.h"
#include "Pathing.h"
#include "MoveData.h"
#include "UnitRulesParam.h"
#include "Command.h"

#include <utility>

namespace circuit {

using namespace springai;

CBuilderManager::CBuilderManager(CCircuitAI* circuit) :
		IUnitModule(circuit),
		builderTasksCount(0),
		builderPower(.0f)
{
	CScheduler* scheduler = circuit->GetScheduler();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::Watchdog, this),
							FRAMES_PER_SEC * 60,
							circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 0);
	// Init after parallel clusterization
	scheduler->RunParallelTask(CGameTask::emptyTask,
							   std::make_shared<CGameTask>(&CBuilderManager::Init, this));

	/*
	 * worker handlers
	 */
	auto workerFinishedHandler = [this](CCircuitUnit* unit) {
		// TODO: Consider moving initilizer into UnitCreated handler
		unit->SetManager(this);
		idleTask->AssignTo(unit);

		builderPower += unit->GetDef()->GetBuildSpeed();
		workers.insert(unit);

		std::vector<float> params;
		params.push_back(3);
		unit->GetUnit()->ExecuteCustomCommand(CMD_RETREAT, params);
	};
	auto workerIdleHandler = [this](CCircuitUnit* unit) {
		// Avoid instant task reassignment, its only valid on build order fail.
		if (this->circuit->GetLastFrame() - unit->GetTaskFrame() > FRAMES_PER_SEC) {
			unit->GetTask()->OnUnitIdle(unit);
		}
	};
	auto workerDamagedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		unit->GetTask()->OnUnitDamaged(unit, attacker);
	};
	auto workerDestroyedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		builderPower -= unit->GetDef()->GetBuildSpeed();
		workers.erase(unit);

		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask
	};

	/*
	 * building handlers
	 */
	auto buildingDestroyedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		Unit* u = unit->GetUnit();
		this->circuit->GetTerrainManager()->RemoveBlocker(unit->GetDef(), u->GetPos(), u->GetBuildingFacing());
	};

	CCircuitAI::UnitDefs& defs = circuit->GetUnitDefs();
	for (auto& kv : defs) {
		UnitDef* def = kv.second;
		if (def->GetSpeed() > 0) {
			if (def->IsBuilder() && !def->GetBuildOptions().empty()) {
				int unitDefId = def->GetUnitDefId();
				finishedHandler[unitDefId] = workerFinishedHandler;
				idleHandler[unitDefId] = workerIdleHandler;
				damagedHandler[unitDefId] = workerDamagedHandler;
				destroyedHandler[unitDefId] = workerDestroyedHandler;
			}
		} else {
			destroyedHandler[def->GetUnitDefId()] = buildingDestroyedHandler;
		}
	}
	// Forbid from removing cormex blocker
	int unitDefId = circuit->GetMexDef()->GetUnitDefId();
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		this->circuit->GetMetalManager()->SetOpenSpot(unit->GetUnit()->GetPos(), true);
	};

	builderTasks.resize(static_cast<int>(IBuilderTask::BuildType::TASKS_COUNT));
}

CBuilderManager::~CBuilderManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (auto& tasks : builderTasks) {
		utils::free_clear(tasks);
	}
}

int CBuilderManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	if (builder == nullptr) {
		return 0; //signaling: OK
	}

	// FIXME: Can IdleTask get here?
	IUnitTask* task = builder->GetTask();
	if (task->GetType() != IUnitTask::Type::BUILDER) {
		return 0; //signaling: OK
	}

	IBuilderTask* taskB = static_cast<IBuilderTask*>(task);
	if (unit->GetUnit()->IsBeingBuilt()) {
		// NOTE: Try to cope with strange event order, when different units created within same task
		// FIXME: Create additional task to catch lost unit
		if (taskB->GetTarget() == nullptr) {
			taskB->SetTarget(unit);  // taskB->SetBuildPos(u->GetPos());
			unfinishedUnits[unit] = taskB;

			UnitDef* buildDef = unit->GetDef();
			Unit* u = unit->GetUnit();
			int facing = u->GetBuildingFacing();
			const AIFloat3& pos = u->GetPos();
			for (auto ass : taskB->GetAssignees()) {
				ass->GetUnit()->Build(buildDef, pos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			}
		}
	} else {
		DequeueTask(taskB);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitFinished(CCircuitUnit* unit)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		DequeueTask(iter->second);
	}

	auto search = finishedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != idleHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	auto search = damagedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != damagedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	if (unit->GetUnit()->IsBeingBuilt()) {
		auto iter = unfinishedUnits.find(unit);
		if (iter != unfinishedUnits.end()) {
			AbortTask(iter->second);
		}
	}

	auto search = destroyedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

float CBuilderManager::GetBuilderPower()
{
	return builderPower;
}

bool CBuilderManager::CanEnqueueTask()
{
	return (builderTasksCount < workers.size() * 4);
}

const std::set<IBuilderTask*>& CBuilderManager::GetTasks(IBuilderTask::BuildType type)
{
	// Auto-creates empty list
	return builderTasks[static_cast<int>(type)];
}

IBuilderTask* CBuilderManager::EnqueueTask(IBuilderTask::Priority priority,
										   UnitDef* buildDef,
										   const AIFloat3& position,
										   IBuilderTask::BuildType type,
										   float cost,
										   int timeout)
{
	return AddTask(priority, buildDef, position, type, cost, timeout);
}

IBuilderTask* CBuilderManager::EnqueueTask(IBuilderTask::Priority priority,
										   UnitDef* buildDef,
										   const AIFloat3& position,
										   IBuilderTask::BuildType type,
										   int timeout)
{
	float cost = buildDef->GetCost(circuit->GetEconomyManager()->GetMetalRes());
	return AddTask(priority, buildDef, position, type, cost, timeout);
}

IBuilderTask* CBuilderManager::EnqueueTask(IBuilderTask::Priority priority,
										   const AIFloat3& position,
										   IBuilderTask::BuildType type,
										   int timeout)
{
	return AddTask(priority, nullptr, position, type, .0f, timeout);
}

IBuilderTask* CBuilderManager::AddTask(IBuilderTask::Priority priority,
									   springai::UnitDef* buildDef,
									   const springai::AIFloat3& position,
									   IBuilderTask::BuildType type,
									   float cost,
									   int timeout)
{
	IBuilderTask* task;
	switch (type) {
		case IBuilderTask::BuildType::FACTORY: {
			task = new CBFactoryTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::NANO: {
			task = new CBNanoTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::STORE: {
			task = new CBStoreTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::PYLON: {
			task = new CBPylonTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::ENERGY: {
			task = new CBEnergyTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::DEFENCE: {
			task = new CBDefenceTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::BUNKER: {
			task = new CBBunkerTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		default:
		case IBuilderTask::BuildType::BIG_GUN: {
			task = new CBBigGunTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::RADAR: {
			task = new CBRadarTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::MEX: {
			task = new CBMexTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::TERRAFORM: {
			// TODO: Re-evalute params
			task = new CBTerraformTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::REPAIR: {
			// TODO: Consider adding target param instead of using CBRepairTask::SetTarget
			task = new CBRepairTask(circuit, priority, timeout);
			break;
		}
		case IBuilderTask::BuildType::RECLAIM: {
			// TODO: Re-evalute params
			task = new CBReclaimTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::PATROL: {
			// TODO: Re-evalute params
			task = new CBPatrolTask(circuit, priority, buildDef, position, cost, timeout);
			break;
		}
	}

	builderTasks[static_cast<int>(type)].insert(task);
	builderTasksCount++;
	// TODO: Send NewTask message
	return task;
}

void CBuilderManager::DequeueTask(IBuilderTask* task)
{
	std::set<IBuilderTask*>& tasks = builderTasks[static_cast<int>(task->GetBuildType())];
	auto it = tasks.find(task);
	if (it != tasks.end()) {
		unfinishedUnits.erase(task->GetTarget());
		task->MarkCompleted();
		tasks.erase(it);
		updateTasks.erase(task);
		delete task;
		builderTasksCount--;
	}
}

void CBuilderManager::AssignTask(CCircuitUnit* unit)
{
	IBuilderTask* task = nullptr;
	Unit* u = unit->GetUnit();
	const AIFloat3& pos = u->GetPos();
	float maxSpeed = u->GetMaxSpeed();
	UnitDef* unitDef = unit->GetDef();
	MoveData* moveData = unitDef->GetMoveData();
	int pathType = moveData->GetPathType();
	delete moveData;
	float buildDistance = unitDef->GetBuildDistance();
	float metric = std::numeric_limits<float>::max();
	for (auto& tasks : builderTasks) {
		for (auto candidate : tasks) {
			if (!candidate->CanAssignTo(unit)) {
				continue;
			}

			// Check time-distance to target
			float weight = 1.0f / (static_cast<float>(candidate->GetPriority()) + 1.0f);
			float dist;
			bool valid;
			CCircuitUnit* target = candidate->GetTarget();
			const AIFloat3& bp = candidate->GetBuildPos();
			if (target != nullptr) {
				Unit* tu = target->GetUnit();

				// FIXME: GetApproximateLength to position occupied by building or feature will return 0.
				//        Also GetApproximateLength could be the cause of lags in late game when simultaneously 30 units become idle
				UnitDef* buildDef = target->GetDef();
				int xsize = buildDef->GetXSize();
				int zsize = buildDef->GetZSize();
				AIFloat3 offset = (pos - bp).Normalize2D() * (sqrtf(xsize * xsize + zsize * zsize) * SQUARE_SIZE + buildDistance);
				AIFloat3 buildPos = candidate->GetBuildPos() + offset;

				dist = circuit->GetPathing()->GetApproximateLength(buildPos, pos, pathType, buildDistance);
				if (dist <= 0) {
//					continue;
					dist = bp.distance(pos) * 1.5;
				}
				if (dist * weight < metric) {
					float maxHealth = tu->GetMaxHealth();
					float healthSpeed = maxHealth * candidate->GetBuildPower() / candidate->GetCost();
					valid = (((maxHealth - tu->GetHealth()) * 0.6) > healthSpeed * (dist / (maxSpeed * FRAMES_PER_SEC)));
				}
			} else {
				dist = circuit->GetPathing()->GetApproximateLength((bp != -RgtVector) ? bp : candidate->GetPos(), pos, pathType, buildDistance);
				if (dist <= 0) {
//					continue;
					dist = bp.distance(pos) * 1.5;
				}
				valid = ((dist * weight < metric) && (dist / (maxSpeed * FRAMES_PER_SEC) < MAX_TRAVEL_SEC));
//				valid = (dist * weight < metric);
			}

			if (valid) {
				task = candidate;
				metric = dist * weight;
			}
		}
	}

	if (task == nullptr) {
		task = circuit->GetEconomyManager()->CreateBuilderTask(unit);
	}

	task->AssignTo(unit);
}

void CBuilderManager::AbortTask(IUnitTask* task)
{
	// NOTE: Don't send Stop command, save some traffic.

	IBuilderTask* taskB = static_cast<IBuilderTask*>(task);
	if (taskB->GetTarget() == nullptr) {
		if (taskB->IsStructure()) {
			circuit->GetTerrainManager()->RemoveBlocker(taskB->GetBuildDef(), taskB->GetBuildPos(), taskB->GetFacing());
		} else if (taskB->GetBuildType() == IBuilderTask::BuildType::MEX) {  // update metalInfo's open state
			circuit->GetMetalManager()->SetOpenSpot(taskB->GetBuildPos(), true);
		}
	}
	DequeueTask(taskB);
}

void CBuilderManager::SpecialCleanUp(CCircuitUnit* unit)
{
	assistants.erase(unit);
}

void CBuilderManager::SpecialProcess(CCircuitUnit* unit)
{
	assistants.insert(unit);
}

void CBuilderManager::FallbackTask(CCircuitUnit* unit)
{
	DequeueTask(static_cast<IBuilderTask*>(unit->GetTask()));

	IBuilderTask* task = EnqueueTask(IBuilderTask::Priority::LOW, unit->GetUnit()->GetPos(), IBuilderTask::BuildType::PATROL, FRAMES_PER_SEC * 20);
	task->AssignTo(unit);
	task->Execute(unit);
}

void CBuilderManager::Init()
{
	CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
	if (commander != nullptr) {
		Unit* u = commander->GetUnit();
		const AIFloat3& pos = u->GetPos();
		UnitRulesParam* param = commander->GetUnit()->GetUnitRulesParamByName("facplop");
		if (param != nullptr) {
			if (param->GetValueFloat() == 1) {
				EnqueueTask(IUnitTask::Priority::HIGH, circuit->GetUnitDefByName("factorycloak"), pos, IBuilderTask::BuildType::FACTORY);
			}
			delete param;
		}

		CMetalManager* metalManager = this->circuit->GetMetalManager();
		int index = metalManager->FindNearestSpot(pos);
		if (index != -1) {
			const CMetalData::Metals& spots = metalManager->GetSpots();
			metalManager->SetOpenSpot(index, false);
			const AIFloat3& buildPos = spots[index].position;
			EnqueueTask(IUnitTask::Priority::NORMAL, circuit->GetMexDef(), buildPos, IBuilderTask::BuildType::MEX)->SetBuildPos(buildPos);
		}
		EnqueueTask(IUnitTask::Priority::NORMAL, circuit->GetUnitDefByName("armsolar"), pos, IBuilderTask::BuildType::ENERGY);
		EnqueueTask(IUnitTask::Priority::NORMAL, circuit->GetUnitDefByName("corrl"), pos, IBuilderTask::BuildType::DEFENCE);
	}
	for (auto worker : workers) {
		UnitIdle(worker);
	}

	CScheduler* scheduler = circuit->GetScheduler();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::UpdateIdle, this), 2, 0);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::UpdateRetreat, this), FRAMES_PER_SEC / 2);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::UpdateBuild, this), 2, 1);
}

void CBuilderManager::Watchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	decltype(assistants)::iterator iter = assistants.begin();
	while (iter != assistants.end()) {
		CCircuitUnit* unit = *iter;
		++iter;
		IBuilderTask* task = static_cast<IBuilderTask*>(unit->GetTask());
		printf("%i\n", task->GetBuildType());
		int timeout = task->GetTimeout();
		if ((timeout > 0) && (circuit->GetLastFrame() - unit->GetTaskFrame() > timeout)) {
			switch (task->GetBuildType()) {
				case IBuilderTask::BuildType::PATROL:
				case IBuilderTask::BuildType::RECLAIM: {
					DequeueTask(task);
//					iter = assistants.erase(iter);
//					continue;
					break;
				}
			}
		}
//		++iter;
	}

	// somehow workers get stuck
	for (auto worker : workers) {
		Unit* u = worker->GetUnit();
		std::vector<springai::Command*> commands = u->GetCurrentCommands();
		// TODO: Ignore workers with idle and wait task? (.. && worker->GetTask()->IsBusy())
		if (commands.empty()) {
			AIFloat3 toPos = u->GetPos();
			const float size = 50.0f;
			CTerrainManager* terrain = circuit->GetTerrainManager();
			toPos.x += (toPos.x > terrain->GetTerrainWidth() / 2) ? -size : size;
			toPos.z += (toPos.z > terrain->GetTerrainHeight() / 2) ? -size : size;
			u->MoveTo(toPos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 10);
		}
		utils::free_clear(commands);
	}

	// find unfinished abandoned buildings
	// TODO: Include special units
	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;
		Unit* u = unit->GetUnit();
		if (u->IsBeingBuilt() && (u->GetMaxSpeed() <= 0) && (unfinishedUnits.find(unit) == unfinishedUnits.end())) {
			const AIFloat3& pos = u->GetPos();
			IBuilderTask* task = EnqueueTask(IBuilderTask::Priority::NORMAL, unit->GetDef(), pos, IBuilderTask::BuildType::REPAIR);
//			task->SetBuildPos(pos);
			task->SetTarget(unit);
			unfinishedUnits[unit] = task;
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
	auto it = updateTasks.begin();
	while ((it != updateTasks.end()) && circuit->IsUpdateTimeValid()) {
		(*it)->Update();
		it = updateTasks.erase(it);
	}

	if (updateTasks.empty()) {
		for (auto& tasks : builderTasks) {
			for (auto t : tasks) {
				updateTasks.insert(t);
			}
		}
	}
}

} // namespace circuit
