/*
 * RepairTask.cpp
 *
 *  Created on: Jan 29, 2015
 *      Author: rlcevg
 */

#include "task/builder/RepairTask.h"
#include "task/RetreatTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"

namespace circuit {

using namespace springai;

CBRepairTask::CBRepairTask(ITaskManager* mgr, Priority priority, CCircuitUnit* target, int timeout)
		: IBuilderTask(mgr, priority, nullptr, -RgtVector, BuildType::REPAIR, 1000.0f, false, timeout)
{
	SetTarget(target);
}

CBRepairTask::~CBRepairTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBRepairTask::RemoveAssignee(CCircuitUnit* unit)
{
	IBuilderTask::RemoveAssignee(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* repTarget = (target != nullptr) ? target : circuit->GetFriendlyUnit(targetId);
	if (repTarget == nullptr) {
		return;
	}
	IUnitTask* task = repTarget->GetTask();
	if ((task == nullptr) || (task->GetType() != IUnitTask::Type::RETREAT)) {
		return;
	}
	CRetreatTask* retTask = static_cast<CRetreatTask*>(task);
	if (retTask->GetRepairer() != unit) {
		return;
	}
	retTask->SetRepairer(nullptr);
	if (!units.empty()) {
		retTask->CheckRepairer(*units.begin());
	}
}

void CBRepairTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* repTarget;
	if (targetId == -1) {
		repTarget = FindUnitToAssist(unit);
		if (repTarget == nullptr) {
			manager->FallbackTask(unit);
			return;
		}
		cost = repTarget->GetCircuitDef()->GetCost();
		targetId = repTarget->GetId();
	} else {
		repTarget = (target != nullptr) ? target : circuit->GetFriendlyUnit(targetId);
	}

	if (repTarget != nullptr) {
		Unit* u = unit->GetUnit();
		u->ExecuteCustomCommand(CMD_PRIORITY, {static_cast<float>(priority)});
		u->Repair(repTarget->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);

		IUnitTask* task = repTarget->GetTask();
		if ((task != nullptr) && (task->GetType() == IUnitTask::Type::RETREAT)) {
			static_cast<CRetreatTask*>(task)->CheckRepairer(unit);
		}
	} else {
		manager->AbortTask(this);
	}
}

void CBRepairTask::Update()
{
	// FIXME: Replace const 1000.0f with build time?
	if ((cost > 1000.0f) && (manager->GetCircuit()->GetEconomyManager()->GetAvgMetalIncome() < savedIncome * 0.6f)) {
		manager->AbortTask(this);
	}
}

void CBRepairTask::Finish()
{
//	CCircuitAI* circuit = manager->GetCircuit();
//	CCircuitUnit* target = circuit->GetFriendlyUnit(targetId);
//	// FIXME: Replace const 1000.0f with build time?
//	if (target != nullptr) {
//		CCircuitDef* cdef = target->GetCircuitDef();
//		if (!cdef->IsMobile() && !cdef->IsAttacker() && (cdef->GetCost() > 1000.0f)) {
//			circuit->GetBuilderManager()->EnqueueTerraform(IBuilderTask::Priority::HIGH, target);
//		}
//	}

	Cancel();
}

void CBRepairTask::Cancel()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* repTarget = (target != nullptr) ? target : circuit->GetFriendlyUnit(targetId);
	if (repTarget == nullptr) {
		return;
	}
	IUnitTask* task = repTarget->GetTask();
	if ((task == nullptr) || (task->GetType() != IUnitTask::Type::RETREAT)) {
		return;
	}
	CRetreatTask* retTask = static_cast<CRetreatTask*>(task);
	CCircuitUnit* repairer = retTask->GetRepairer();
	for (CCircuitUnit* unit : units) {
		if (repairer == unit) {
			retTask->SetRepairer(nullptr);
			break;
		}
	}
}

void CBRepairTask::OnUnitIdle(CCircuitUnit* unit)
{
	Unit* u = target->GetUnit();
	if (u->GetHealth() < u->GetMaxHealth()) {
		// unit stuck or event order fail
		RemoveAssignee(unit);
	} else {
		// task finished
		manager->DoneTask(this);
	}
}

void CBRepairTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	Unit* u = unit->GetUnit();
	if (u->GetHealth() >= u->GetMaxHealth() * unit->GetCircuitDef()->GetRetreat()) {
		return;
	}

	CRetreatTask* task = manager->GetCircuit()->GetBuilderManager()->EnqueueRetreat();
	manager->AssignTask(unit, task);
}

void CBRepairTask::SetTarget(CCircuitUnit* unit)
{
	if (unit != nullptr) {
		CCircuitAI* circuit = manager->GetCircuit();
		target = circuit->GetTeamUnit(unit->GetId());
		cost = unit->GetCircuitDef()->GetCost();
		position = buildPos = unit->GetPos(circuit->GetLastFrame());
		circuit->GetTerrainManager()->CorrectPosition(buildPos);  // position will contain non-corrected value
		targetId = unit->GetId();
//		buildDef = unit->GetCircuitDef();
		if (!unit->GetUnit()->IsBeingBuilt()) {
			savedIncome = .0f;
		}
	} else {
		target = nullptr;
		cost = 1000.0f;
		position = buildPos = -RgtVector;
		targetId = -1;
//		buildDef = nullptr;
	}
}

CCircuitUnit* CBRepairTask::FindUnitToAssist(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* target = nullptr;
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	float maxSpeed = unit->GetUnit()->GetMaxSpeed();
	float radius = unit->GetCircuitDef()->GetBuildDistance() + maxSpeed * FRAMES_PER_SEC * 30;

	circuit->UpdateFriendlyUnits();
	auto units = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius));
	for (Unit* u : units) {
		if ((u != nullptr) && u->GetHealth() < u->GetMaxHealth() && u->GetVel().Length() <= maxSpeed * 1.5f) {
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
