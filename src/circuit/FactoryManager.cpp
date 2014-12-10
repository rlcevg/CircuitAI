/*
 * FactoryManager.cpp
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#include "FactoryManager.h"
#include "CircuitAI.h"
#include "Scheduler.h"
#include "CircuitUnit.h"
#include "FactoryTask.h"
#include "EconomyManager.h"
#include "TerrainManager.h"
#include "utils.h"

#include "AIFloat3.h"
#include "AISCommands.h"
#include "OOAICallback.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Command.h"

#include <vector>

namespace circuit {

using namespace springai;

CFactoryManager::CFactoryManager(CCircuitAI* circuit) :
		IModule(circuit),
		factoryPower(.0f),
		assistDef(nullptr)
{
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::Watchdog, this),
										  FRAMES_PER_SEC * 60,
										  circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 1);

	/*
	 * factory handlers
	 */
	auto factoryFinishedHandler = [this](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		UnitDef* def = unit->GetDef();
		AIFloat3 pos = u->GetPos();
		factoryPower += def->GetBuildSpeed();

		// check nanos around
		if (assistDef != nullptr) {
			std::list<CCircuitUnit*> nanos;
			float radius = assistDef->GetBuildDistance();
			std::vector<Unit*> units = this->circuit->GetCallback()->GetFriendlyUnitsIn(u->GetPos(), radius);
			int nanoId = assistDef->GetUnitDefId();
			int teamId = this->circuit->GetTeamId();
			for (auto nano : units) {
				UnitDef* ndef = nano->GetDef();
				if (ndef->GetUnitDefId() == nanoId && nano->GetTeam() == teamId) {
					nanos.push_back(this->circuit->GetUnitById(nano->GetUnitId()));
				}
				delete ndef;
			}
			utils::free_clear(units);
			factories[unit] = nanos;
		}

		// try to avoid factory stuck
		float size = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE * 0.75;
		CTerrainManager* terrain = this->circuit->GetTerrainManager();
		pos.x += (pos.x > terrain->GetTerrainWidth() / 2) ? -size : size;
		pos.z += (pos.z > terrain->GetTerrainHeight() / 2) ? -size : size;
		u->MoveTo(pos);

		AssignTask(unit);
		ExecuteTask(unit);
	};
	auto factoryIdleHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			AssignTask(unit);
		}
		ExecuteTask(unit);
	};
	auto factoryDestroyedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		factoryPower -= unit->GetDef()->GetBuildSpeed();
		factories.erase(unit);
		unit->RemoveTask();
	};

	/*
	 * armnanotc handlers
	 */
	auto assistFinishedHandler = [this](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		UnitDef* def = unit->GetDef();
		factoryPower += def->GetBuildSpeed();
		const AIFloat3& fromPos = u->GetPos();
		AIFloat3 toPos = fromPos;
		float size = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE;
		CTerrainManager* terrain = this->circuit->GetTerrainManager();
		toPos.x += (toPos.x > terrain->GetTerrainWidth() / 2) ? -size : size;
		toPos.z += (toPos.z > terrain->GetTerrainHeight() / 2) ? -size : size;
		u->PatrolTo(toPos);

		std::vector<float> params;
		params.push_back(0.0f);
		u->ExecuteCustomCommand(CMD_PRIORITY, params);

		// check to which factory nano belongs to
		float radius = def->GetBuildDistance();
		float qradius = radius * radius;
		for (auto& fac : factories) {
			const AIFloat3& facPos = fac.first->GetUnit()->GetPos();
			if (facPos.SqDistance2D(fromPos) < qradius) {
				fac.second.push_back(unit);
			}
		}
	};
	auto assistDestroyedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		factoryPower -= unit->GetDef()->GetBuildSpeed();
		for (auto& fac : factories) {
			fac.second.remove(unit);
		}
	};

	CCircuitAI::UnitDefs& defs = circuit->GetUnitDefs();
	for (auto& kv : defs) {
		UnitDef* def = kv.second;
		if (def->IsBuilder() && (def->GetSpeed() == 0)) {
			int unitDefId = def->GetUnitDefId();
			if  (!def->GetBuildOptions().empty()) {
				finishedHandler[unitDefId] = factoryFinishedHandler;
				idleHandler[unitDefId] = factoryIdleHandler;
				destroyedHandler[unitDefId] = factoryDestroyedHandler;
			} else {
				finishedHandler[unitDefId] = assistFinishedHandler;
				destroyedHandler[unitDefId] = assistDestroyedHandler;
				assistDef = def;
			}
		}
	}
}

CFactoryManager::~CFactoryManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(factoryTasks);
}

int CFactoryManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	if ((builder != nullptr) && unit->GetUnit()->IsBeingBuilt()) {
		IConstructTask* task = static_cast<IConstructTask*>(builder->GetTask());
		if ((task != nullptr) && (task->GetConstructType() == IConstructTask::ConstructType::FACTORY)) {
			CFactoryTask* taskF = static_cast<CFactoryTask*>(task);
			unfinishedTasks[taskF].push_back(unit);
			unfinishedUnits[unit] = taskF;
		}
	}

	return 0; //signaling: OK
}

int CFactoryManager::UnitFinished(CCircuitUnit* unit)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		CFactoryTask* task = iter->second;
		if (task != nullptr) {
			task->Progress();
			std::list<CCircuitUnit*>& units = unfinishedTasks[task];
			if (task->IsDone()) {
				task->MarkCompleted();
				factoryTasks.remove(task);
				for (auto u : units) {
					unfinishedUnits[u] = nullptr;
				}
				unfinishedTasks.erase(task);
				delete task;
			} else {
				units.remove(unit);
			}
		}
		unfinishedUnits.erase(iter);
	}

	auto search = finishedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CFactoryManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != idleHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CFactoryManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	if (unit->GetUnit()->IsBeingBuilt()) {
		auto iter = unfinishedUnits.find(unit);
		if (iter != unfinishedUnits.end()) {
			CFactoryTask* task = iter->second;
			if (task != nullptr) {
				std::list<CCircuitUnit*>& units = unfinishedTasks[task];
				units.remove(iter->first);
				if (units.empty()) {
					unfinishedTasks.erase(task);
				}
				task->Regress();
			}
			unfinishedUnits.erase(iter);
		}
	}

	auto search = destroyedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

CFactoryTask* CFactoryManager::EnqueueTask(CFactoryTask::Priority priority,
										   UnitDef* buildDef,
										   const AIFloat3& position,
										   CFactoryTask::TaskType type,
										   int quantity,
										   float radius)
{
	CFactoryTask* task = new CFactoryTask(priority, buildDef, position, type, quantity, radius);
	factoryTasks.push_front(task);
	return task;
}

void CFactoryManager::DequeueTask(CFactoryTask* task)
{
	std::list<CCircuitUnit*>& units = unfinishedTasks[task];
	task->MarkCompleted();
	factoryTasks.remove(task);
	for (auto u : units) {
		unfinishedUnits[u] = nullptr;
	}
	unfinishedTasks.erase(task);
	delete task;
}

float CFactoryManager::GetFactoryPower()
{
	return factoryPower;
}

bool CFactoryManager::CanEnqueueTask()
{
	return (factoryTasks.size() < factories.size() * 2);
}

const std::list<CFactoryTask*>& CFactoryManager::GetTasks() const
{
	return factoryTasks;
}

CCircuitUnit* CFactoryManager::NeedUpgrade()
{
	// TODO: Wrap into predicate
	if (assistDef != nullptr) {
		for (auto& fac : factories) {
			if (fac.second.size() < 4) {
				return fac.first;
			}
		}
	}
	return nullptr;
}

CCircuitUnit* CFactoryManager::GetRandomFactory()
{
	auto iter = factories.begin();
	std::advance(iter, rand() % factories.size());
	return iter->first;
}

void CFactoryManager::Watchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (auto& fac : factories) {
		CCircuitUnit* unit = fac.first;
		Unit* u = unit->GetUnit();
		std::vector<springai::Command*> commands = u->GetCurrentCommands();
		if (commands.empty()) {
			UnitIdle(unit);
		}
		utils::free_clear(commands);
	}
}

void CFactoryManager::AssignTask(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	UnitDef* def = unit->GetDef();

	CFactoryTask* task = nullptr;
	decltype(factoryTasks)::iterator iter = factoryTasks.begin();
	for (; iter != factoryTasks.end(); ++iter) {
		if ((*iter)->CanAssignTo(unit)) {
			task = static_cast<CFactoryTask*>(*iter);
			break;
		}
	}

	if (task == nullptr) {
		task = circuit->GetEconomyManager()->CreateFactoryTask(unit);

//		iter = factoryTasks.begin();
	}

	task->AssignTo(unit);
//	if (task->IsFull()) {
//		factoryTasks.splice(factoryTasks.end(), factoryTasks, iter);  // move task to back
//	}
}

void CFactoryManager::ExecuteTask(CCircuitUnit* unit)
{
	CFactoryTask* task = static_cast<CFactoryTask*>(unit->GetTask());
	Unit* u = unit->GetUnit();
	const AIFloat3& buildPos = u->GetPos();

	UnitDef* buildDef = task->GetBuildDef();
	u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING);
}

} // namespace circuit
