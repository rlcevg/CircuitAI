/*
 * RepairTask.cpp
 *
 *  Created on: Sep 4, 2016
 *      Author: rlcevg
 */

#include "task/common/RepairTask.h"
#include "task/RetreatTask.h"
#include "task/TaskManager.h"
//#include "module/BuilderManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "AISCommands.h"

namespace circuit {

using namespace springai;

IRepairTask::IRepairTask(ITaskManager* mgr, Priority priority, Type type, CAllyUnit* target, int timeout)
		: IBuilderTask(mgr, priority, nullptr, -RgtVector, type, BuildType::REPAIR, 1000.0f, 0.f, timeout)
{
	SetTarget(target);
}

IRepairTask::~IRepairTask()
{
}

void IRepairTask::RemoveAssignee(CCircuitUnit* unit)
{
	IBuilderTask::RemoveAssignee(unit);

	// Inform repair-target task/unit
	CCircuitAI* circuit = manager->GetCircuit();
	CAllyUnit* repTarget = (target != nullptr) ? target : circuit->GetFriendlyUnit(targetId);
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

void IRepairTask::Finish()
{
//	CCircuitAI* circuit = manager->GetCircuit();
//	CAllyUnit* target = circuit->GetFriendlyUnit(targetId);
//	// FIXME: Replace const 1000.0f with build time?
//	if (target != nullptr) {
//		CCircuitDef* cdef = target->GetCircuitDef();
//		if (!cdef->IsMobile() && !cdef->IsAttacker() && (cdef->GetCost() > 1000.0f)) {
//			circuit->GetBuilderManager()->EnqueueTerraform(IBuilderTask::Priority::HIGH, target);
//		}
//	}

	Cancel();
}

void IRepairTask::Cancel()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CAllyUnit* repTarget = (target != nullptr) ? target : circuit->GetFriendlyUnit(targetId);
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

void IRepairTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CAllyUnit* repTarget;
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

	if ((repTarget != nullptr) && (repTarget->GetUnit()->GetHealth() < repTarget->GetUnit()->GetMaxHealth())) {
		Unit* u = unit->GetUnit();
		TRY_UNIT(circuit, unit,
			u->ExecuteCustomCommand(CMD_PRIORITY, {ClampPriority()});
			u->Repair(repTarget->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
		)

		IUnitTask* task = repTarget->GetTask();
		if ((task != nullptr) && (task->GetType() == IUnitTask::Type::RETREAT)) {
			static_cast<CRetreatTask*>(task)->CheckRepairer(unit);
		}
	} else {
		manager->AbortTask(this);
	}
}

void IRepairTask::SetTarget(CAllyUnit* unit)
{
	if (unit != nullptr) {
		CCircuitAI* circuit = manager->GetCircuit();
		target = circuit->GetTeamUnit(unit->GetId());
		cost = unit->GetCircuitDef()->GetCost();
		position = buildPos = unit->GetPos(circuit->GetLastFrame());
//		CTerrainManager::CorrectPosition(buildPos);  // position will contain non-corrected value
		targetId = unit->GetId();
		if (unit->GetUnit()->IsBeingBuilt()) {
			buildDef = unit->GetCircuitDef();
		} else {
			savedIncome = .0f;
		}
	} else {
		target = nullptr;
		cost = 1000.0f;
		position = buildPos = -RgtVector;
		targetId = -1;
		buildDef = nullptr;
	}
}

CAllyUnit* IRepairTask::FindUnitToAssist(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CAllyUnit* target = nullptr;
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	float maxSpeed = unit->GetCircuitDef()->GetSpeed();
	float radius = unit->GetCircuitDef()->GetBuildDistance() + maxSpeed * 30;
	maxSpeed = SQUARE(maxSpeed * 1.5f / FRAMES_PER_SEC);

	circuit->UpdateFriendlyUnits();
	auto units = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius));
	for (Unit* u : units) {
		if ((u != nullptr) && (u->GetHealth() < u->GetMaxHealth()) && (u->GetVel().SqLength2D() <= maxSpeed)) {
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
