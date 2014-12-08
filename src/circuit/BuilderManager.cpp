/*
 * BuilderManager.cpp
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#include "BuilderManager.h"
#include "CircuitAI.h"
#include "Scheduler.h"
#include "GameAttribute.h"
#include "MetalManager.h"
#include "CircuitUnit.h"
#include "EconomyManager.h"
#include "utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Map.h"
#include "Pathing.h"
#include "MoveData.h"
#include "UnitRulesParam.h"
//#include "Drawer.h"

namespace circuit {

using namespace springai;

CBuilderManager::CBuilderManager(CCircuitAI* circuit) :
		IModule(circuit),
		builderTasksCount(0),
		builderPower(.0f),
		economyManager(nullptr)
{
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::Watchdog, this),
										  FRAMES_PER_SEC * 60,
										  circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 0);
	// Init after parallel clusterization
	circuit->GetScheduler()->RunParallelTask(std::make_shared<CGameTask>([](){return;}),
											 std::make_shared<CGameTask>(&CBuilderManager::Init, this));

	/*
	 * worker handlers
	 */
	auto workerFinishedHandler = [this](CCircuitUnit* unit) {
		builderPower += unit->GetDef()->GetBuildSpeed();
		workers.insert(unit);
	};
	auto workerIdleHandler = [this](CCircuitUnit* unit) {
		CBuilderTask* task = static_cast<CBuilderTask*>(unit->GetTask());
		if ((task != nullptr) && (task->GetType() == CBuilderTask::TaskType::ASSIST)) {
			task->SetTarget(nullptr);
		} else {
			unit->RemoveTask();
			AssignTask(unit);
		}
		ExecuteTask(unit);
	};
	auto workerDestroyedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		builderPower -= unit->GetDef()->GetBuildSpeed();
		workers.erase(unit);
		builderInfo.erase(unit);
		CBuilderTask* task = static_cast<CBuilderTask*>(unit->GetTask());
		if (task != nullptr) {
			DequeueTask(task);
		}
	};

	CCircuitAI::UnitDefs& defs = circuit->GetUnitDefs();
	for (auto& pair : defs) {
		UnitDef* def = pair.second;
		if (def->IsBuilder() && !def->GetBuildOptions().empty() && (def->GetSpeed() > 0)) {
			int unitDefId = def->GetUnitDefId();
			finishedHandler[unitDefId] = workerFinishedHandler;
			idleHandler[unitDefId] = workerIdleHandler;
			destroyedHandler[unitDefId] = workerDestroyedHandler;
		}
	}
}

CBuilderManager::~CBuilderManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (auto& tasks : builderTasks) {
		utils::free_clear(tasks.second);
	}
}

void CBuilderManager::SetEconomyManager(CEconomyManager* ecoMgr)
{
	economyManager = ecoMgr;
}

int CBuilderManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	if ((builder != nullptr) && unit->GetUnit()->IsBeingBuilt()) {
		IConstructTask* task = static_cast<IConstructTask*>(builder->GetTask());
		if ((task != nullptr) && (task->GetConstructType() == IConstructTask::ConstructType::BUILDER)) {
			CBuilderTask* taskB = static_cast<CBuilderTask*>(task);
			// NOTE: Try to cope with strange event order, when different units created within same task
			if (taskB->GetTarget() == nullptr) {
				taskB->SetTarget(unit);
				unfinishedUnits[unit] = taskB;
			}
			for (auto ass : taskB->GetAssignees()) {
				ass->GetUnit()->Repair(unit->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			}
		}
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitFinished(CCircuitUnit* unit)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		CBuilderTask* task = iter->second;
		// FIXME: Is this check necessary?
		if (task != nullptr) {
			DequeueTask(task);
		} else {
			unfinishedUnits.erase(iter);
		}
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

int CBuilderManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	if (unit->GetUnit()->IsBeingBuilt()) {
		auto iter = unfinishedUnits.find(unit);
		if (iter != unfinishedUnits.end()) {
			CBuilderTask* task = iter->second;
			// FIXME: Is this check necessary?
			if (task != nullptr) {
				DequeueTask(task);
			} else {
				unfinishedUnits.erase(iter);
			}
		}
	}

	auto search = destroyedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

CBuilderTask* CBuilderManager::EnqueueTask(CBuilderTask::Priority priority,
										   UnitDef* buildDef,
										   const AIFloat3& position,
										   CBuilderTask::TaskType type,
										   float cost,
										   int timeout)
{
	CBuilderTask* task = new CBuilderTask(priority, buildDef, position, type, cost, timeout);
	builderTasks[type].push_front(task);
	builderTasksCount++;
	return task;
}

CBuilderTask* CBuilderManager::EnqueueTask(CBuilderTask::Priority priority,
										   UnitDef* buildDef,
										   const AIFloat3& position,
										   CBuilderTask::TaskType type,
										   int timeout)
{
	float cost = buildDef->GetCost(economyManager->GetMetalRes());
	CBuilderTask* task = new CBuilderTask(priority, buildDef, position, type, cost, timeout);
	builderTasks[type].push_front(task);
	builderTasksCount++;
	return task;
}

CBuilderTask* CBuilderManager::EnqueueTask(CBuilderTask::Priority priority,
										   const AIFloat3& position,
										   CBuilderTask::TaskType type,
										   int timeout)
{
	CBuilderTask* task = new CBuilderTask(priority, nullptr, position, type, 1000.0f, timeout);
	builderTasks[type].push_front(task);
	builderTasksCount++;
	return task;
}

void CBuilderManager::DequeueTask(CBuilderTask* task)
{
	unfinishedUnits.erase(task->GetTarget());
	task->MarkCompleted();
	builderTasks[task->GetType()].remove(task);
	delete task;
	builderTasksCount--;
}

float CBuilderManager::GetBuilderPower()
{
	return builderPower;
}

bool CBuilderManager::CanEnqueueTask()
{
	return (builderTasksCount < workers.size() * 2);
}

const std::list<CBuilderTask*>& CBuilderManager::GetTasks(CBuilderTask::TaskType type)
{
	// Auto-creates empty list
	return builderTasks[type];
}

void CBuilderManager::Init()
{
	// TODO: Improve init
	for (auto worker : workers) {
		if (circuit->GetCommander() != worker) {
			UnitIdle(worker);
		} else {
			Unit* u = worker->GetUnit();
			UnitRulesParam* param = worker->GetUnit()->GetUnitRulesParamByName("facplop");
			if (param != nullptr) {
				if (param->GetValueFloat() == 1) {
					const AIFloat3& position = u->GetPos();
					int facing = UNIT_COMMAND_BUILD_NO_FACING;
					float terWidth = circuit->GetTerrainWidth();
					float terHeight = circuit->GetTerrainHeight();
					if (math::fabs(terWidth - 2 * position.x) > math::fabs(terHeight - 2 * position.z)) {
						if (2 * position.x > terWidth) {
							facing = UNIT_FACING_WEST;
						} else {
							facing = UNIT_FACING_EAST;
						}
					} else {
						if (2 * position.z > terHeight) {
							facing = UNIT_FACING_NORTH;
						} else {
							facing = UNIT_FACING_SOUTH;
						}
					}
					UnitDef* buildDef = circuit->GetUnitDefByName("factorycloak");
					AIFloat3 buildPos = circuit->FindBuildSiteSpace(buildDef, position, 1000.0f, facing);
					u->Build(buildDef, buildPos, facing);
				}
				delete param;
			}
		}
	}
}

void CBuilderManager::Watchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	decltype(builderInfo)::iterator iter = builderInfo.begin();
	while (iter != builderInfo.end()) {
		CBuilderTask* task = static_cast<CBuilderTask*>(iter->first->GetTask());
		if (task != nullptr) {
			int timeout = task->GetTimeout();
			if ((timeout > 0) && (circuit->GetLastFrame() - iter->second.startFrame > timeout)) {
				switch (task->GetType()) {
					case CBuilderTask::TaskType::ASSIST: {
						CCircuitUnit* unit = iter->first;
						task->MarkCompleted();
						auto search = builderTasks.find(CBuilderTask::TaskType::ASSIST);
						if (search != builderTasks.end()) {
							search->second.remove(task);
						}
						delete task;
						unit->GetUnit()->Stop();
						iter = builderInfo.erase(iter);
						continue;
						break;
					}
				}
			}
		}
		++iter;
	}

	// somehow workers get stuck
	for (auto worker : workers) {
		Unit* u = worker->GetUnit();
		Resource* metalRes = economyManager->GetMetalRes();
		if ((u->GetVel() == ZeroVector) && (u->GetResourceUse(metalRes) == 0)) {
			AIFloat3 toPos = u->GetPos();
			const float size = 50.0f;
			toPos.x += (toPos.x > circuit->GetTerrainWidth() / 2) ? -size : size;
			toPos.z += (toPos.z > circuit->GetTerrainHeight() / 2) ? -size : size;
			u->MoveTo(toPos);
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
		for (auto& t : tasks.second) {
			if (!t->CanAssignTo(unit)) {
				continue;
			}
			CBuilderTask* candidate = static_cast<CBuilderTask*>(t);

			// Check time-distance to target
			float dist;
			bool valid;
			CCircuitUnit* target = candidate->GetTarget();
			if (target != nullptr) {
				Unit* tu = target->GetUnit();
				dist = circuit->GetPathing()->GetApproximateLength(candidate->GetBuildPos(), pos, pathType, buildDistance);
				if (dist < metric) {
					float maxHealth = tu->GetMaxHealth();
					float healthSpeed = maxHealth * candidate->GetBuildPower() / candidate->GetCost();
					valid = (((maxHealth - tu->GetHealth()) * 0.8) > healthSpeed * (dist / (maxSpeed * FRAMES_PER_SEC)));
				}
			} else {
				const AIFloat3& bp = candidate->GetBuildPos();
				dist = circuit->GetPathing()->GetApproximateLength((bp != -RgtVector) ? bp : candidate->GetPos(), pos, pathType, buildDistance);
				valid = dist < metric;
			}

			if (valid) {
				task = candidate;
				metric = dist;
			}
		}
	}

	if (task == nullptr) {
		task = economyManager->CreateBuilderTask(unit);
	}

	task->AssignTo(unit);
}

void CBuilderManager::ExecuteTask(CCircuitUnit* unit)
{
	CBuilderTask* task = static_cast<CBuilderTask*>(unit->GetTask());
	Unit* u = unit->GetUnit();

	auto findFacing = [this](UnitDef* buildDef, const AIFloat3& position) {
		int facing = UNIT_COMMAND_BUILD_NO_FACING;
		float terWidth = circuit->GetTerrainWidth();
		float terHeight = circuit->GetTerrainHeight();
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
		CBuilderTask* taskNew = new CBuilderTask(CBuilderTask::Priority::LOW, nullptr, pos, CBuilderTask::TaskType::ASSIST, 1000, FRAMES_PER_SEC * 20);
		taskNew->AssignTo(unit);

		const float size = SQUARE_SIZE * 10;
		pos.x += (pos.x > circuit->GetTerrainWidth() / 2) ? -size : size;
		pos.z += (pos.z > circuit->GetTerrainHeight() / 2) ? -size : size;
		u->PatrolTo(pos);

		builderInfo[unit].startFrame = circuit->GetLastFrame();
	};

	CBuilderTask::TaskType type = task->GetType();
	switch (type) {
//		case CBuilderTask::TaskType::FACTORY:
//		case CBuilderTask::TaskType::NANO:
//		case CBuilderTask::TaskType::EXPAND:
//		case CBuilderTask::TaskType::SOLAR:
//		case CBuilderTask::TaskType::FUSION:
//		case CBuilderTask::TaskType::SINGU:
//		case CBuilderTask::TaskType::PYLON:
//		case CBuilderTask::TaskType::DEFENDER:
//		case CBuilderTask::TaskType::DDM:
//		case CBuilderTask::TaskType::ANNI:
		default: {
			std::vector<float> params;
			params.push_back(static_cast<float>(task->GetPriority()));
			u->ExecuteCustomCommand(CMD_PRIORITY, params);

			if (task->GetTarget() != nullptr) {
				u->Repair(task->GetTarget()->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
				break;
			}
			UnitDef* buildDef = task->GetBuildDef();
			AIFloat3 buildPos = task->GetBuildPos();
			int facing = UNIT_COMMAND_BUILD_NO_FACING;
			if (buildPos != -RgtVector) {
				facing = findFacing(buildDef, buildPos);
				if (circuit->GetMap()->IsPossibleToBuildAt(buildDef, buildPos, facing)) {
					u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
					break;
				}
			}

			bool valid = false;
			switch (type) {
				case CBuilderTask::TaskType::EXPAND:
				case CBuilderTask::TaskType::PYLON: {
					buildPos = economyManager->FindBuildPos(unit);
					valid = (buildPos != -RgtVector);
					break;
				}
				case CBuilderTask::TaskType::NANO: {
					const AIFloat3& position = task->GetPos();
					float searchRadius = buildDef->GetBuildDistance();
					facing = findFacing(buildDef, position);
					buildPos = circuit->FindBuildSite(buildDef, position, searchRadius, facing);
					if (buildPos == -RgtVector) {
						// TODO: Replace FindNearestSpots with FindNearestClusters
						const CMetalManager::Metals& spots =  circuit->GetGameAttribute()->GetMetalManager().GetSpots();
						CMetalManager::MetalIndices indices = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpots(position, 3);
						for (const int idx : indices) {
							facing = findFacing(buildDef, spots[idx].position);
							buildPos = circuit->FindBuildSite(buildDef, spots[idx].position, searchRadius, facing);
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
					buildPos = circuit->FindBuildSiteSpace(buildDef, position, searchRadius, facing);
					if (buildPos == -RgtVector) {
						// TODO: Replace FindNearestSpots with FindNearestClusters
						const CMetalManager::Metals& spots =  circuit->GetGameAttribute()->GetMetalManager().GetSpots();
						CMetalManager::MetalIndices indices = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpots(position, 3);
						for (const int idx : indices) {
							facing = findFacing(buildDef, spots[idx].position);
							buildPos = circuit->FindBuildSiteSpace(buildDef, spots[idx].position, searchRadius, facing);
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
				u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			} else {
				// Fallback to Guard/Assist/Patrol
				assistFallback(unit);
			}
			break;
		}
		case CBuilderTask::TaskType::ASSIST: {
			std::vector<float> params;
			params.push_back(2.0f);
			u->ExecuteCustomCommand(CMD_PRIORITY, params);

			CCircuitUnit* target = task->GetTarget();
			if (target == nullptr) {
				target = FindUnitToAssist(unit);
				if (target == nullptr) {
					assistFallback(unit);
					break;
				}
			}
			unit->GetUnit()->Repair(target->GetUnit(), UNIT_COMMAND_OPTION_SHIFT_KEY, FRAMES_PER_SEC * 60);
			auto search = builderInfo.find(unit);
			if (search == builderInfo.end()) {
				builderInfo[unit].startFrame = circuit->GetLastFrame();
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
	float radius = unit->GetDef()->GetBuildDistance() + maxSpeed * FRAMES_PER_SEC * 5;
	std::vector<Unit*> units = circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius);
	for (auto u : units) {
		if (u->GetHealth() < u->GetMaxHealth() && u->GetSpeed() <= maxSpeed * 2) {
			target = circuit->GetUnitById(u->GetUnitId());
			if (target != nullptr) {
				break;
			}
		}
	}
	utils::free_clear(units);
	return target;
}

} // namespace circuit
