/*
 * BuilderManager.cpp
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#include "module/BuilderManager.h"
#include "CircuitAI.h"
#include "static/SetupManager.h"
#include "static/MetalManager.h"
#include "unit/CircuitUnit.h"
#include "module/EconomyManager.h"
#include "module/FactoryManager.h"
#include "terrain/TerrainManager.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Map.h"
#include "Pathing.h"
#include "MoveData.h"
#include "UnitRulesParam.h"
#include "Command.h"

namespace circuit {

using namespace springai;

CBuilderManager::CBuilderManager(CCircuitAI* circuit) :
		IModule(circuit),
		builderTasksCount(0),
		builderPower(.0f)
{
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::Watchdog, this),
										  FRAMES_PER_SEC * 60,
										  circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 0);
	// Init after parallel clusterization
	circuit->GetScheduler()->RunParallelTask(CGameTask::EmptyTask,
											 std::make_shared<CGameTask>(&CBuilderManager::Init, this));

	/*
	 * worker handlers
	 */
	auto workerFinishedHandler = [this](CCircuitUnit* unit) {
		builderPower += unit->GetDef()->GetBuildSpeed();
		workers.insert(unit);

		std::vector<float> params;
		params.push_back(3);
		unit->GetUnit()->ExecuteCustomCommand(CMD_RETREAT, params);
	};
	auto workerIdleHandler = [this](CCircuitUnit* unit) {
		CBuilderTask* task = static_cast<CBuilderTask*>(unit->GetTask());
		if ((task != nullptr) && (task->GetType() == CBuilderTask::TaskType::ASSIST)) {
			DequeueTask(task);
		} else {
			unit->RemoveTask();
		}
		AssignTask(unit);
		ExecuteTask(unit);
	};
	auto workerDamagedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		builderInfos.erase(unit);
		CBuilderTask* task = static_cast<CBuilderTask*>(unit->GetTask());
		if (task != nullptr) {
			if (task->GetTarget() == nullptr) {
				AbandonTask(task, unit);
			} else {
				unit->RemoveTask();
			}
			CCircuitUnit* haven = this->circuit->GetFactoryManager()->GetClosestHaven(unit);
			const AIFloat3& pos = (haven != nullptr) ? haven->GetUnit()->GetPos() : this->circuit->GetSetupManager()->GetStartPos();
			unit->GetUnit()->MoveTo(pos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 1);
		}
	};
	auto workerDestroyedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		builderPower -= unit->GetDef()->GetBuildSpeed();
		workers.erase(unit);
		builderInfos.erase(unit);
		CBuilderTask* task = static_cast<CBuilderTask*>(unit->GetTask());
		if (task != nullptr) {
			AbandonTask(task, unit);
		}
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

	builderTasks.resize(static_cast<int>(CBuilderTask::TaskType::TASKS_COUNT));
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

	IConstructTask* task = static_cast<IConstructTask*>(builder->GetTask());
	if ((task == nullptr) || (task->GetConstructType() != IConstructTask::ConstructType::BUILDER)) {
		return 0; //signaling: OK
	}

	CBuilderTask* taskB = static_cast<CBuilderTask*>(task);
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
			AbandonTask(iter->second);
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

const std::list<CBuilderTask*>& CBuilderManager::GetTasks(CBuilderTask::TaskType type)
{
	// Auto-creates empty list
	return builderTasks[static_cast<int>(type)];
}

CBuilderTask* CBuilderManager::EnqueueTask(CBuilderTask::Priority priority,
										   UnitDef* buildDef,
										   const AIFloat3& position,
										   CBuilderTask::TaskType type,
										   float cost,
										   int timeout)
{
	CBuilderTask* task = new CBuilderTask(priority, buildDef, position, type, cost, timeout);
	builderTasks[static_cast<int>(type)].push_front(task);
	builderTasksCount++;
	return task;
}

CBuilderTask* CBuilderManager::EnqueueTask(CBuilderTask::Priority priority,
										   UnitDef* buildDef,
										   const AIFloat3& position,
										   CBuilderTask::TaskType type,
										   int timeout)
{
	float cost = buildDef->GetCost(circuit->GetEconomyManager()->GetMetalRes());
	CBuilderTask* task = new CBuilderTask(priority, buildDef, position, type, cost, timeout);
	builderTasks[static_cast<int>(type)].push_front(task);
	builderTasksCount++;
	return task;
}

CBuilderTask* CBuilderManager::EnqueueTask(CBuilderTask::Priority priority,
										   const AIFloat3& position,
										   CBuilderTask::TaskType type,
										   int timeout)
{
	CBuilderTask* task = new CBuilderTask(priority, nullptr, position, type, .0f, timeout);
	builderTasks[static_cast<int>(type)].push_front(task);
	builderTasksCount++;
	return task;
}

void CBuilderManager::DequeueTask(CBuilderTask* task)
{
	unfinishedUnits.erase(task->GetTarget());
	task->MarkCompleted();
	builderTasks[static_cast<int>(task->GetType())].remove(task);
	delete task;
	builderTasksCount--;
}

void CBuilderManager::AbandonTask(CBuilderTask* task, CCircuitUnit* unit)
{
	for (auto ass : task->GetAssignees()) {
		if (ass != unit) {
			ass->GetUnit()->Stop();
		}
	}
	if (task->GetTarget() == nullptr) {
		if (task->IsStructure()) {
			circuit->GetTerrainManager()->RemoveBlocker(task->GetBuildDef(), task->GetBuildPos(), task->GetFacing());
		} else if (task->GetType() == CBuilderTask::TaskType::EXPAND) {  // update metalInfo's open state
			circuit->GetMetalManager()->SetOpenSpot(task->GetBuildPos(), true);
		}
	}
	DequeueTask(task);
}

void CBuilderManager::Init()
{
	CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
	if (commander != nullptr) {
		Unit* u = commander->GetUnit();
		UnitRulesParam* param = commander->GetUnit()->GetUnitRulesParamByName("facplop");
		if (param != nullptr) {
			if (param->GetValueFloat() == 1) {
				const AIFloat3& pos = u->GetPos();
				EnqueueTask(IUnitTask::Priority::HIGH, circuit->GetUnitDefByName("factorycloak"), pos, CBuilderTask::TaskType::FACTORY);
				CMetalManager* metalManager = this->circuit->GetMetalManager();
				int index = metalManager->FindNearestSpot(pos);
				if (index != -1) {
					const CMetalData::Metals& spots = metalManager->GetSpots();
					metalManager->SetOpenSpot(index, false);
					const AIFloat3& buildPos = spots[index].position;
					EnqueueTask(IUnitTask::Priority::NORMAL, circuit->GetMexDef(), buildPos, CBuilderTask::TaskType::EXPAND)->SetBuildPos(buildPos);
				}
				EnqueueTask(IUnitTask::Priority::NORMAL, circuit->GetUnitDefByName("armsolar"), pos, CBuilderTask::TaskType::SOLAR);
				EnqueueTask(IUnitTask::Priority::NORMAL, circuit->GetUnitDefByName("corrl"), pos, CBuilderTask::TaskType::DEFENDER);
			}
			delete param;
		}
	}
	for (auto worker : workers) {
		UnitIdle(worker);
	}
}

void CBuilderManager::Watchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	decltype(builderInfos)::iterator iter = builderInfos.begin();
	while (iter != builderInfos.end()) {
		CBuilderTask* task = static_cast<CBuilderTask*>(iter->first->GetTask());
		if (task == nullptr) {
			iter->first->GetUnit()->Stop();
			iter = builderInfos.erase(iter);
			continue;
		}
		int timeout = task->GetTimeout();
		if ((timeout > 0) && (circuit->GetLastFrame() - iter->second.startFrame > timeout)) {
			switch (task->GetType()) {
				case CBuilderTask::TaskType::PATROL:
				case CBuilderTask::TaskType::RECLAIM: {
					CCircuitUnit* unit = iter->first;
					DequeueTask(task);
					unit->GetUnit()->Stop();
					iter = builderInfos.erase(iter);
					continue;
					break;
				}
			}
		}
		++iter;
	}

	// somehow workers get stuck
	for (auto worker : workers) {
		Unit* u = worker->GetUnit();
		std::vector<springai::Command*> commands = u->GetCurrentCommands();
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
			CBuilderTask* task = EnqueueTask(CBuilderTask::Priority::NORMAL, unit->GetDef(), pos, CBuilderTask::TaskType::ASSIST);
			task->SetBuildPos(pos);
			task->SetTarget(unit);
			unfinishedUnits[unit] = task;
		}
	}
}

void CBuilderManager::AssignTask(CCircuitUnit* unit)
{
	CBuilderTask* task = nullptr;
	Unit* u = unit->GetUnit();
	const AIFloat3& pos = u->GetPos();
	float maxSpeed = u->GetMaxSpeed();
	MoveData* moveData = unit->GetDef()->GetMoveData();
	int pathType = moveData->GetPathType();
	delete moveData;
	float buildDistance = unit->GetDef()->GetBuildDistance();
	float metric = std::numeric_limits<float>::max();
	for (auto& tasks : builderTasks) {
		for (auto& t : tasks) {
			if (!t->CanAssignTo(unit)) {
				continue;
			}
			CBuilderTask* candidate = static_cast<CBuilderTask*>(t);

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
				int facing = tu->GetBuildingFacing();
				int xsize = ((facing & 1) == 0) ? buildDef->GetXSize() : buildDef->GetZSize();
				int zsize = ((facing & 1) == 1) ? buildDef->GetXSize() : buildDef->GetZSize();
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

void CBuilderManager::ExecuteTask(CCircuitUnit* unit)
{
	CBuilderTask* task = static_cast<CBuilderTask*>(unit->GetTask());
	Unit* u = unit->GetUnit();

	auto findFacing = [this](UnitDef* buildDef, const AIFloat3& position) {
		int facing = UNIT_COMMAND_BUILD_NO_FACING;
		CTerrainManager* terrain = circuit->GetTerrainManager();
		float terWidth = terrain->GetTerrainWidth();
		float terHeight = terrain->GetTerrainHeight();
		if (math::fabs(terWidth - 2 * position.x) > math::fabs(terHeight - 2 * position.z)) {
			facing = (2 * position.x > terWidth) ? UNIT_FACING_WEST : UNIT_FACING_EAST;
		} else {
			facing = (2 * position.z > terHeight) ? UNIT_FACING_NORTH : UNIT_FACING_SOUTH;
		}
		return facing;
	};

	auto assistFallback = [this, task, u](CCircuitUnit* unit) {
		DequeueTask(task);

		std::vector<float> params;
		params.push_back(0.0f);
		u->ExecuteCustomCommand(CMD_PRIORITY, params);

		AIFloat3 pos = u->GetPos();
		CBuilderTask* taskNew = EnqueueTask(CBuilderTask::Priority::LOW, pos, CBuilderTask::TaskType::PATROL, FRAMES_PER_SEC * 20);
		taskNew->AssignTo(unit);

		const float size = SQUARE_SIZE * 10;
		CTerrainManager* terrain = circuit->GetTerrainManager();
		pos.x += (pos.x > terrain->GetTerrainWidth() / 2) ? -size : size;
		pos.z += (pos.z > terrain->GetTerrainHeight() / 2) ? -size : size;
		u->PatrolTo(pos);

		builderInfos[unit].startFrame = circuit->GetLastFrame();
	};

	std::vector<float> params;
	params.push_back(static_cast<float>(task->GetPriority()));
	u->ExecuteCustomCommand(CMD_PRIORITY, params);

	CBuilderTask::TaskType type = task->GetType();
	switch (type) {
//		case CBuilderTask::TaskType::FACTORY:
//		case CBuilderTask::TaskType::NANO:
//		case CBuilderTask::TaskType::SOLAR:
//		case CBuilderTask::TaskType::FUSION:
//		case CBuilderTask::TaskType::SINGU:
//		case CBuilderTask::TaskType::PYLON:
//		case CBuilderTask::TaskType::DEFENDER:
//		case CBuilderTask::TaskType::DDM:
//		case CBuilderTask::TaskType::ANNI:
//		case CBuilderTask::TaskType::RAVE:
		default: {
			CCircuitUnit* target = task->GetTarget();
			if (target != nullptr) {
				Unit* tu = target->GetUnit();
				u->Build(target->GetDef(), tu->GetPos(), tu->GetBuildingFacing(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
				break;
			}
			int facing = UNIT_COMMAND_BUILD_NO_FACING;
			UnitDef* buildDef = task->GetBuildDef();
			AIFloat3 buildPos = task->GetBuildPos();
			if (buildPos != -RgtVector) {
				facing = findFacing(buildDef, buildPos);
				if (circuit->GetMap()->IsPossibleToBuildAt(buildDef, buildPos, facing)) {
					u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
					break;
				} else {
					circuit->GetTerrainManager()->RemoveBlocker(buildDef, buildPos, facing);
				}
			}

			bool valid = false;
			switch (type) {
				case CBuilderTask::TaskType::PYLON: {
					buildPos = circuit->GetEconomyManager()->FindBuildPos(unit);
					valid = (buildPos != -RgtVector);
					break;
				}
				case CBuilderTask::TaskType::NANO: {
					const AIFloat3& position = task->GetPos();
					float searchRadius = buildDef->GetBuildDistance();
					facing = findFacing(buildDef, position);
					CTerrainManager* terrain = circuit->GetTerrainManager();
					buildPos = terrain->FindBuildSite(buildDef, position, searchRadius, facing);
					if (buildPos == -RgtVector) {
						// TODO: Replace FindNearestSpots with FindNearestClusters
						const CMetalData::Metals& spots =  circuit->GetMetalManager()->GetSpots();
						CMetalData::MetalIndices indices = circuit->GetMetalManager()->FindNearestSpots(position, 3);
						for (const int idx : indices) {
							facing = findFacing(buildDef, spots[idx].position);
							buildPos = terrain->FindBuildSite(buildDef, spots[idx].position, searchRadius, facing);
							if (buildPos != -RgtVector) {
								break;
							}
						}
					}
					valid = (buildPos != -RgtVector);
					break;
				}
				default: {
					const AIFloat3& position = task->GetPos();
					float searchRadius = 100.0f * SQUARE_SIZE;
					facing = findFacing(buildDef, position);
					CTerrainManager* terrain = circuit->GetTerrainManager();
					buildPos = terrain->FindBuildSite(buildDef, position, searchRadius, facing);
					if (buildPos == -RgtVector) {
						// TODO: Replace FindNearestSpots with FindNearestClusters
						const CMetalData::Metals& spots =  circuit->GetMetalManager()->GetSpots();
						CMetalData::MetalIndices indices = circuit->GetMetalManager()->FindNearestSpots(position, 3);
						for (const int idx : indices) {
							facing = findFacing(buildDef, spots[idx].position);
							buildPos = terrain->FindBuildSite(buildDef, spots[idx].position, searchRadius, facing);
							if (buildPos != -RgtVector) {
								break;
							}
						}
					}
					valid = (buildPos != -RgtVector);
					break;
				}
			}

			if (valid) {
				task->SetBuildPos(buildPos);
				task->SetFacing(facing);
				circuit->GetTerrainManager()->AddBlocker(buildDef, buildPos, facing);
				u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			} else {
				// Fallback to Guard/Assist/Patrol
				assistFallback(unit);
			}
			break;
		}
		case CBuilderTask::TaskType::EXPAND: {
			CCircuitUnit* target = task->GetTarget();
			if (target != nullptr) {
				Unit* tu = target->GetUnit();
				u->Build(target->GetDef(), tu->GetPos(), tu->GetBuildingFacing(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
				break;
			}
			int facing = UNIT_COMMAND_BUILD_NO_FACING;
			UnitDef* buildDef = task->GetBuildDef();
			AIFloat3 buildPos = task->GetBuildPos();
			if (buildPos != -RgtVector) {
				if (circuit->GetMap()->IsPossibleToBuildAt(buildDef, buildPos, facing)) {
					u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
					break;
				} else {
					circuit->GetMetalManager()->SetOpenSpot(buildPos, true);
				}
			}

			buildPos = circuit->GetEconomyManager()->FindBuildPos(unit);
			if (buildPos != -RgtVector) {
				task->SetBuildPos(buildPos);
				circuit->GetMetalManager()->SetOpenSpot(buildPos, false);
				u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			} else {
				// Fallback to Guard/Assist/Patrol
				assistFallback(unit);
			}
			break;
		}
		case CBuilderTask::TaskType::ASSIST: {
			CCircuitUnit* target = task->GetTarget();
			if (target == nullptr) {
				target = FindUnitToAssist(unit);
				if (target == nullptr) {
					assistFallback(unit);
					break;
				}
			}
			unit->GetUnit()->Repair(target->GetUnit(), UNIT_COMMAND_OPTION_SHIFT_KEY, FRAMES_PER_SEC * 60);
			auto search = builderInfos.find(unit);
			if (search == builderInfos.end()) {
				builderInfos[unit].startFrame = circuit->GetLastFrame();
			}
			break;
		}
		case CBuilderTask::TaskType::RECLAIM: {
			CTerrainManager* terrain = circuit->GetTerrainManager();
			float width = terrain->GetTerrainWidth() / 2;
			float height = terrain->GetTerrainHeight() / 2;
			AIFloat3 pos(width, 0, height);
			float radius = sqrtf(width * width + height * height);
			unit->GetUnit()->ReclaimInArea(pos, radius, UNIT_COMMAND_OPTION_SHIFT_KEY, FRAMES_PER_SEC * 60);
			auto search = builderInfos.find(unit);
			if (search == builderInfos.end()) {
				builderInfos[unit].startFrame = circuit->GetLastFrame();
			}
			break;
		}
	}
}

CCircuitUnit* CBuilderManager::FindUnitToAssist(CCircuitUnit* unit)
{
	CCircuitUnit* target = nullptr;
	Unit* su = unit->GetUnit();
	const AIFloat3& pos = su->GetPos();
	float maxSpeed = su->GetMaxSpeed();
	float radius = unit->GetDef()->GetBuildDistance() + maxSpeed * FRAMES_PER_SEC * 10;
	circuit->UpdateAllyUnits();
	std::vector<Unit*> units = circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius);
	for (auto u : units) {
		if (u->GetHealth() < u->GetMaxHealth() && u->GetSpeed() <= maxSpeed * 2) {
			target = circuit->GetFriendlyUnit(u);
			if (target != nullptr) {
				break;
			}
		}
	}
	utils::free_clear(units);
	return target;
}

} // namespace circuit
