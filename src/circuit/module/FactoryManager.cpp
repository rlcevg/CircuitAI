/*
 * FactoryManager.cpp
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#include "module/FactoryManager.h"
#include "module/EconomyManager.h"
#include "terrain/TerrainManager.h"
#include "task/IdleTask.h"
#include "task/static/RepairTask.h"
#include "task/static/ReclaimTask.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AIFloat3.h"
#include "OOAICallback.h"
#include "Command.h"

#include <vector>

namespace circuit {

using namespace springai;

CFactoryManager::CFactoryManager(CCircuitAI* circuit) :
		IUnitModule(circuit),
		factoryPower(.0f),
		assistDef(nullptr)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::Watchdog, this),
							FRAMES_PER_SEC * 60,
							circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 1);
	const int interval = FRAMES_PER_SEC;
	const int offset = circuit->GetSkirmishAIId() % interval;
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::UpdateIdle, this), interval, offset + 10);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::UpdateAssist, this), interval, offset + 11);

	/*
	 * factory handlers
	 */
	auto factoryFinishedHandler = [this](CCircuitUnit* unit) {
		unit->SetManager(this);
		idleTask->AssignTo(unit);

		Unit* u = unit->GetUnit();
		UnitDef* def = unit->GetCircuitDef()->GetUnitDef();
		AIFloat3 pos = u->GetPos();
		factoryPower += def->GetBuildSpeed();

		// check nanos around
		std::set<CCircuitUnit*> nanos;
		float radius = assistDef->GetBuildDistance();
		auto units = std::move(this->circuit->GetCallback()->GetFriendlyUnitsIn(u->GetPos(), radius));
		int nanoId = assistDef->GetId();
		int teamId = this->circuit->GetTeamId();
		for (auto nano : units) {
			if (nano == nullptr) {
				continue;
			}
			UnitDef* ndef = nano->GetDef();
			if (ndef->GetUnitDefId() == nanoId && nano->GetTeam() == teamId) {
				nanos.insert(this->circuit->GetTeamUnit(nano->GetUnitId()));
			}
			delete ndef;
		}
		utils::free_clear(units);
		factories[unit] = nanos;
	};
	auto factoryIdleHandler = [this](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto factoryDestroyedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		factoryPower -= unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
		factories.erase(unit);

		unit->GetTask()->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask
	};

	/*
	 * armnanotc handlers
	 */
	auto assistFinishedHandler = [this](CCircuitUnit* unit) {
		unit->SetManager(this);
		idleTask->AssignTo(unit);

		Unit* u = unit->GetUnit();
		factoryPower += unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
		const AIFloat3& pos = u->GetPos();

		std::vector<float> params;
		params.push_back(0.0f);
		u->ExecuteCustomCommand(CMD_PRIORITY, params);

		// check factory nano belongs to
		float radius = unit->GetCircuitDef()->GetBuildDistance();
		float qradius = radius * radius;
		for (auto& fac : factories) {
			const AIFloat3& facPos = fac.first->GetUnit()->GetPos();
			if (facPos.SqDistance2D(pos) < qradius) {
				fac.second.insert(unit);
			}
		}

		havens.insert(unit);
		// TODO: Send HavenFinished message
	};
	auto assistIdleHandler = [this](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto assistDestroyedHandler = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		factoryPower -= unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
		for (auto& fac : factories) {
			fac.second.erase(unit);
		}

		havens.erase(unit);
		// TODO: Send HavenDestroyed message

		unit->GetTask()->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask
	};

	CCircuitAI::CircuitDefs& defs = circuit->GetCircuitDefs();
	for (auto& kv : defs) {
		CCircuitDef* cdef = kv.second;
		UnitDef* def = cdef->GetUnitDef();
		if (def->IsBuilder() && (def->GetSpeed() == 0)) {
			int unitDefId = kv.first;
			if  (!kv.second->GetBuildOptions().empty()) {
				finishedHandler[unitDefId] = factoryFinishedHandler;
				idleHandler[unitDefId] = factoryIdleHandler;
				destroyedHandler[unitDefId] = factoryDestroyedHandler;
			} else {
				finishedHandler[unitDefId] = assistFinishedHandler;
				idleHandler[unitDefId] = assistIdleHandler;
				destroyedHandler[unitDefId] = assistDestroyedHandler;
				assistDef = cdef;
			}
		}
	}

	// FIXME: EXPERIMENTAL
	/*
	 * striderhub handlers
	 */
	CCircuitDef::Id defId = circuit->GetCircuitDef("striderhub")->GetId();
	finishedHandler[defId] = [this, defId](CCircuitUnit* unit) {
		unit->SetManager(this);

		Unit* u = unit->GetUnit();
		UnitDef* def = unit->GetCircuitDef()->GetUnitDef();
		AIFloat3 pos = u->GetPos();
		factoryPower += def->GetBuildSpeed();
		CRecruitTask* task = new CRecruitTask(this, IUnitTask::Priority::HIGH, nullptr, ZeroVector, CRecruitTask::BuildType::FIREPOWER, unit->GetCircuitDef()->GetBuildDistance());
		unit->SetTask(task);

		// check nanos around
		std::set<CCircuitUnit*> nanos;
		float radius = assistDef->GetBuildDistance();
		auto units = std::move(this->circuit->GetCallback()->GetFriendlyUnitsIn(u->GetPos(), radius));
		int nanoId = assistDef->GetId();
		int teamId = this->circuit->GetTeamId();
		for (auto nano : units) {
			if (nano == nullptr) {
				continue;
			}
			UnitDef* ndef = nano->GetDef();
			if (ndef->GetUnitDefId() == nanoId && nano->GetTeam() == teamId) {
				nanos.insert(this->circuit->GetTeamUnit(nano->GetUnitId()));
			}
			delete ndef;
		}
		utils::free_clear(units);
		factories[unit] = nanos;

		std::vector<float> params;
		params.push_back(2.0f);
		u->ExecuteCustomCommand(CMD_PRIORITY, params);

//		u->SetRepeat(true);
		idleHandler[defId](unit);
	};
	idleHandler[defId] = [this](CCircuitUnit* unit) {
		AIFloat3 pos = unit->GetUnit()->GetPos();
		CTerrainManager* terrain = this->circuit->GetTerrainManager();
		CCircuitDef* detriDef = this->circuit->GetCircuitDef("armorco");
		pos = terrain->FindBuildSite(detriDef, pos, this->circuit->GetCircuitDef("striderhub")->GetBuildDistance(), -1);
		if (pos != -RgtVector) {
			unit->GetUnit()->Build(detriDef->GetUnitDef(), pos, -1, 0, FRAMES_PER_SEC * 10);
		}
	};
	destroyedHandler[defId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		factoryPower -= unit->GetCircuitDef()->GetUnitDef()->GetBuildSpeed();
		factories.erase(unit);
		delete unit->GetTask();
	};
	// FIXME: EXPERIMENTAL
}

CFactoryManager::~CFactoryManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(factoryTasks);
	utils::free_clear(deleteTasks);
	utils::free_clear(assistTasks);
	utils::free_clear(deleteAssists);
}

int CFactoryManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	if (builder == nullptr) {
		return 0; //signaling: OK
	}

	IUnitTask* task = builder->GetTask();
	if (task->GetType() != IUnitTask::Type::FACTORY) {
		return 0; //signaling: OK
	}

	if (unit->GetUnit()->IsBeingBuilt()) {
		CRecruitTask* taskR = static_cast<CRecruitTask*>(task);
		if (taskR->GetTarget() == nullptr) {
			taskR->SetTarget(unit);
			unfinishedUnits[unit] = taskR;
		}
	}

	return 0; //signaling: OK
}

int CFactoryManager::UnitFinished(CCircuitUnit* unit)
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

int CFactoryManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetCircuitDef()->GetId());
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
			AbortTask(iter->second);
		}
	}

	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

CRecruitTask* CFactoryManager::EnqueueTask(CRecruitTask::Priority priority,
										   CCircuitDef* buildDef,
										   const AIFloat3& position,
										   CRecruitTask::BuildType type,
										   float radius)
{
	CRecruitTask* task = new CRecruitTask(this, priority, buildDef, position, type, radius);
	factoryTasks.insert(task);
	return task;
}

IBuilderTask* CFactoryManager::EnqueueReclaim(IBuilderTask::Priority priority,
											  const springai::AIFloat3& position,
											  float radius,
											  int timeout)
{
	IBuilderTask* task = new CSReclaimTask(this, priority, position, .0f, timeout, radius);
	assistTasks.insert(task);
	return task;
}

IBuilderTask* CFactoryManager::EnqueueRepair(IBuilderTask::Priority priority,
											 CCircuitUnit* target)
{
	IBuilderTask* task = new CSRepairTask(this, priority, target);
	assistTasks.insert(task);
	return task;
}

void CFactoryManager::DequeueTask(IUnitTask* task, bool done)
{
	// TODO: Convert CRecruitTask into IBuilderTask
	auto itf = factoryTasks.find(static_cast<CRecruitTask*>(task));
	if (itf != factoryTasks.end()) {
		unfinishedUnits.erase(static_cast<CRecruitTask*>(task)->GetTarget());
		factoryTasks.erase(itf);
		task->Close(done);
		deleteTasks.insert(static_cast<CRecruitTask*>(task));
		return;
	}
	auto ita = assistTasks.find(static_cast<IBuilderTask*>(task));
	if (ita != assistTasks.end()) {
		assistTasks.erase(ita);
		task->Close(done);
		deleteAssists.insert(static_cast<IBuilderTask*>(task));
	}
}

void CFactoryManager::AssignTask(CCircuitUnit* unit)
{
	if (unit->GetCircuitDef() == assistDef) {  // FIXME: Check Id instead pointers?
		IBuilderTask* task = circuit->GetEconomyManager()->CreateAssistTask(unit);
		if (task != nullptr) {  // if nullptr then continue to Wait (or Idle)
			task->AssignTo(unit);
		}

	} else {

		CRecruitTask* task = nullptr;
		decltype(factoryTasks)::iterator iter = factoryTasks.begin();
		for (; iter != factoryTasks.end(); ++iter) {
			if ((*iter)->CanAssignTo(unit)) {
				task = static_cast<CRecruitTask*>(*iter);
				break;
			}
		}

		if (task == nullptr) {
			task = circuit->GetEconomyManager()->CreateFactoryTask(unit);

//			iter = factoryTasks.begin();
		}

		task->AssignTo(unit);
//		if (task->IsFull()) {
//			factoryTasks.splice(factoryTasks.end(), factoryTasks, iter);  // move task to back
//		}
	}
}

void CFactoryManager::AbortTask(IUnitTask* task)
{
	DequeueTask(task, false);
}

void CFactoryManager::DoneTask(IUnitTask* task)
{
	DequeueTask(task, true);
}

void CFactoryManager::SpecialCleanUp(CCircuitUnit* unit)
{
}

void CFactoryManager::SpecialProcess(CCircuitUnit* unit)
{
}

void CFactoryManager::FallbackTask(CCircuitUnit* unit)
{
}

float CFactoryManager::GetFactoryPower()
{
	return factoryPower;
}

bool CFactoryManager::CanEnqueueTask()
{
	return (factoryTasks.size() < factories.size() * 2);
}

const std::set<CRecruitTask*>& CFactoryManager::GetTasks() const
{
	return factoryTasks;
}

CCircuitUnit* CFactoryManager::NeedUpgrade()
{
	// TODO: Wrap into predicate
	if (assistDef != nullptr) {
		for (auto& fac : factories) {
			if (fac.second.size() < 9) {
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

CCircuitDef* CFactoryManager::GetAssistDef() const
{
	return assistDef;
}

CCircuitUnit* CFactoryManager::GetClosestHaven(CCircuitUnit* unit) const
{
	if (havens.empty()) {
		return nullptr;
	}
	CCircuitUnit* haven = nullptr;
	float metric = std::numeric_limits<float>::max();
	const AIFloat3& position = unit->GetUnit()->GetPos();
	CTerrainManager* terrain = circuit->GetTerrainManager();
	for (auto hav : havens) {
		const AIFloat3& pos = hav->GetUnit()->GetPos();
		if (!terrain->CanMoveToPos(unit->GetArea(), pos)) {
			continue;
		}
		float qdist = pos.SqDistance2D(position);
		if (qdist < metric) {
			haven = hav;
			metric = qdist;
		}
	}
	return haven;
}

std::vector<CCircuitUnit*> CFactoryManager::GetHavensAt(const AIFloat3& pos) const
{
	std::vector<CCircuitUnit*> result;
	result.reserve(havens.size());  // size overkill
	float sqBuildDist = assistDef->GetBuildDistance();
	sqBuildDist *= sqBuildDist;
	for (auto haven : havens) {
		if (haven->GetUnit()->GetPos().SqDistance2D(pos) <= sqBuildDist) {
			result.push_back(haven);
		}
	}
	return result;
}

void CFactoryManager::Watchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	auto checkIdler = [this](CCircuitUnit* unit) {
		auto commands = std::move(unit->GetUnit()->GetCurrentCommands());
		if (commands.empty()) {
			UnitIdle(unit);
		}
		utils::free_clear(commands);
	};

	for (auto& fac : factories) {
		checkIdler(fac.first);
	}

	for (auto haven : havens) {
		checkIdler(haven);
	}
}

void CFactoryManager::UpdateIdle()
{
	idleTask->Update();
}

void CFactoryManager::UpdateAssist()
{
	utils::free_clear(deleteTasks);
	if (!deleteAssists.empty()) {
		for (auto task : deleteAssists) {
			updateAssists.erase(task);
			delete task;
		}
		deleteAssists.clear();
	}

	auto it = updateAssists.begin();
	while (it != updateAssists.end()) {
		(*it)->Update();
		it = updateAssists.erase(it);
		if (circuit->IsUpdateTimeValid()) {
			break;
		}
	}

	if (updateAssists.empty()) {
		updateAssists.insert(assistTasks.begin(), assistTasks.end());
	}
}

} // namespace circuit
