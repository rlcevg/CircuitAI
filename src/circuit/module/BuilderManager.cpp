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
#include "static/TerrainData.h"
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
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
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
	auto workerFinishedHandler = [this](CCircuitUnit* unit) {
		// TODO: Consider moving initilizer into UnitCreated handler
		unit->SetManager(this);
		idleTask->AssignTo(unit);
		++buildAreas[unit->GetArea()][unit->GetCircuitDef()];

		builderPower += unit->GetDef()->GetBuildSpeed();
		workers.insert(unit);

//		std::vector<float> params;
//		params.push_back(3);
//		unit->GetUnit()->ExecuteCustomCommand(CMD_RETREAT, params);

		AddBuildList(unit);
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
		--buildAreas[unit->GetArea()][unit->GetCircuitDef()];

		builderPower -= unit->GetDef()->GetBuildSpeed();
		workers.erase(unit);

		unit->GetTask()->OnUnitDestroyed(unit, attacker);
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		RemoveBuildList(unit);
	};

	/*
	 * building handlers
	 */
	auto buildingDestroyedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		Unit* u = unit->GetUnit();
		this->circuit->GetTerrainManager()->RemoveBlocker(unit->GetDef(), u->GetPos(), u->GetBuildingFacing());
	};

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CCircuitAI::UnitDefs& defs = circuit->GetUnitDefs();
	for (auto& kv : defs) {
		UnitDef* def = kv.second;
		if (def->GetSpeed() > 0) {
			if (def->IsBuilder() && !circuit->GetCircuitDef(def)->GetBuildOptions().empty()) {
				int unitDefId = def->GetUnitDefId();
				finishedHandler[unitDefId] = workerFinishedHandler;
				idleHandler[unitDefId] = workerIdleHandler;
				damagedHandler[unitDefId] = workerDamagedHandler;
				destroyedHandler[unitDefId] = workerDestroyedHandler;

				int mtId = terrainManager->GetMobileTypeId(unitDefId);
				if (mtId >= 0) {  // not air
					workerMobileTypes.insert(mtId);
				}
				workerDefs.insert(def);
			}
		} else {
			destroyedHandler[def->GetUnitDefId()] = buildingDestroyedHandler;
		}
	}

	// Forbid from removing cormex blocker
	int unitDefId = circuit->GetEconomyManager()->GetMexDef()->GetUnitDefId();
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
//		this->circuit->GetMetalManager()->SetOpenSpot(unit->GetUnit()->GetPos(), true);
//		this->circuit->GetEconomyManager()->UpdateMetalTasks(this->circuit->GetSetupManager()->GetStartPos());
		const AIFloat3& pos = unit->GetUnit()->GetPos();
		if (IsBuilderInArea(unit->GetDef(), pos)) {
			EnqueueTask(IBuilderTask::Priority::HIGH, unit->GetDef(), pos, IBuilderTask::BuildType::MEX)->SetBuildPos(pos);
		}
	};

	builderTasks.resize(static_cast<int>(IBuilderTask::BuildType::TASKS_COUNT));

	for (auto mtId : workerMobileTypes) {
		for (auto area : terrainManager->GetMobileTypeById(mtId)->area) {
			buildAreas[area] = std::map<CCircuitDef*, int>();
		}
	}
	buildAreas[nullptr] = std::map<CCircuitDef*, int>();  // air
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
			taskB->SetTarget(unit);
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

void CBuilderManager::AddBuildList(CCircuitUnit* unit)
{
	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->GetCount() > 1) {
		return;
	}

	const std::unordered_set<UnitDef*>& buildOptions = cdef->GetBuildOptions();
	std::set<UnitDef*> buildDefs;
	for (auto build : buildOptions) {
		buildDefs.insert(build);
		circuit->GetCircuitDef(build)->IncBuild();
	}

	circuit->GetEconomyManager()->AddAvailEnergy(buildDefs);

	// TODO: Same thing with factory, etc.
}

void CBuilderManager::RemoveBuildList(CCircuitUnit* unit)
{
	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->GetCount() > 1) {
		return;
	}

	const std::unordered_set<UnitDef*>& buildOptions = cdef->GetBuildOptions();
	std::set<UnitDef*> buildDefs;
	for (auto build : buildOptions) {
		buildDefs.insert(build);
		CCircuitDef* cdef = circuit->GetCircuitDef(build);
		cdef->DecBuild();
		if (cdef->GetCount() == 0) {
			buildDefs.insert(build);
		}
	}

	circuit->GetEconomyManager()->RemoveAvailEnergy(buildDefs);

	// TODO: Same thing with factory, etc.
}

float CBuilderManager::GetBuilderPower()
{
	return builderPower;
}

bool CBuilderManager::CanEnqueueTask()
{
	return (builderTasksCount < workers.size() * 2);
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
			task = new CBFactoryTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::NANO: {
			task = new CBNanoTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::STORE: {
			task = new CBStoreTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::PYLON: {
			task = new CBPylonTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::ENERGY: {
			task = new CBEnergyTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::DEFENCE: {
			task = new CBDefenceTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::BUNKER: {
			task = new CBBunkerTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		default:
		case IBuilderTask::BuildType::BIG_GUN: {
			task = new CBBigGunTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::RADAR: {
			task = new CBRadarTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::MEX: {
			task = new CBMexTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::TERRAFORM: {
			// TODO: Re-evalute params
			task = new CBTerraformTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::REPAIR: {
			// TODO: Consider adding target param instead of using CBRepairTask::SetTarget
			task = new CBRepairTask(this, priority, timeout);
			break;
		}
		case IBuilderTask::BuildType::RECLAIM: {
			// TODO: Re-evalute params
			task = new CBReclaimTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
		case IBuilderTask::BuildType::PATROL: {
			// TODO: Re-evalute params
			task = new CBPatrolTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
	}

	builderTasks[static_cast<int>(type)].insert(task);
	builderTasksCount++;
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
		updateTasks.erase(task);
		deleteTasks.insert(task);
		builderTasksCount--;
	}
}

bool CBuilderManager::IsBuilderInArea(UnitDef* buildDef, const AIFloat3& position)
{
	// check air first
	for (auto& kv : buildAreas[nullptr]) {
		if ((kv.second > 0) && kv.first->CanBuild(buildDef)) {
			return true;
		}
	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	int iS = terrainManager->GetSectorIndex(position);
	for (auto mtId : workerMobileTypes) {
		STerrainMapArea* area = terrainManager->GetMobileTypeById(mtId)->sector[iS].area;
		if (area != nullptr) {
			for (auto& kv : buildAreas[area]) {
				if ((kv.second > 0) && kv.first->CanBuild(buildDef)) {
					return true;
				}
			}
		}
	}
	return false;
}

void CBuilderManager::AssignTask(CCircuitUnit* unit)
{
	IBuilderTask* task = nullptr;
	Unit* u = unit->GetUnit();
	const AIFloat3& pos = u->GetPos();
	float maxSpeed = u->GetMaxSpeed();
	UnitDef* unitDef = unit->GetDef();
	float buildDistance = unitDef->GetBuildDistance();
	MoveData* moveData = unitDef->GetMoveData();
	int pathType = moveData->GetPathType();
	delete moveData;
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
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
			bool valid;

			CCircuitUnit* target = candidate->GetTarget();
			if (target != nullptr) {
				const AIFloat3& bp = candidate->GetBuildPos();

				if (!terrainManager->CanBuildAt(unit, bp)) {
					continue;
				}

				// FIXME: GetApproximateLength to position occupied by building or feature will return 0.
				UnitDef* buildDef = target->GetDef();
				int xsize = buildDef->GetXSize();
				int zsize = buildDef->GetZSize();
				AIFloat3 offset = (pos - bp).Normalize2D() * (sqrtf(xsize * xsize + zsize * zsize) * SQUARE_SIZE + buildDistance);
				const AIFloat3& buildPos = bp + offset;

				Unit* tu = target->GetUnit();
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

				const AIFloat3& buildPos = candidate->GetPosition();

				if (!terrainManager->CanBuildAt(unit, buildPos)) {
					continue;
				}

				dist = circuit->GetPathing()->GetApproximateLength(buildPos, pos, pathType, buildDistance);
				if (dist <= 0) {
//					continue;
					dist = buildPos.distance(pos) * 1.5;
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
	DequeueTask(static_cast<IBuilderTask*>(task), false);
}

void CBuilderManager::DoneTask(IUnitTask* task)
{
	DequeueTask(static_cast<IBuilderTask*>(task), true);
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

		// FIXME: Remove, make proper eco rules!
		CMetalManager* metalManager = this->circuit->GetMetalManager();
		int index = metalManager->FindNearestSpot(pos);
		if (index != -1) {
			const CMetalData::Metals& spots = metalManager->GetSpots();
			metalManager->SetOpenSpot(index, false);
			const AIFloat3& buildPos = spots[index].position;
			EnqueueTask(IUnitTask::Priority::NORMAL, circuit->GetEconomyManager()->GetMexDef(), buildPos, IBuilderTask::BuildType::MEX)->SetBuildPos(buildPos);
		}
	}
	for (auto worker : workers) {
		UnitIdle(worker);
	}

	CScheduler* scheduler = circuit->GetScheduler().get();
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
		int timeout = task->GetTimeout();
		if ((timeout > 0) && (circuit->GetLastFrame() - unit->GetTaskFrame() > timeout)) {
			switch (task->GetBuildType()) {
				case IBuilderTask::BuildType::PATROL:
				case IBuilderTask::BuildType::RECLAIM: {
					DoneTask(task);
					break;
				}
			}
		}
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
			task->SetTarget(unit);
			unfinishedUnits[unit] = task;
		}
	}

	// scheduled task deletion
	utils::free_clear(deleteTasks);
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
			updateTasks.insert(tasks.begin(), tasks.end());
		}
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
		if (workerDefs.find(unit->GetDef()) != workerDefs.end()) {
			++buildAreas[unit->GetArea()][unit->GetCircuitDef()];
		}
	}

	std::set<IBuilderTask*> removeTasks;
	for (auto& tasks : builderTasks) {
		for (auto task : tasks) {
			// FIXME: Do not remove tasks if there is builder around, able to build it (even in another area)
			UnitDef* def = task->GetBuildDef();
			if ((def != nullptr) && !IsBuilderInArea(def, task->GetPosition())) {
				removeTasks.insert(task);
			}
		}
	}
	for (auto task : removeTasks) {
		AbortTask(task);
	}
}

} // namespace circuit
