/*
 * RepairTask.cpp
 *
 *  Created on: Jan 29, 2015
 *      Author: rlcevg
 */

#include "task/builder/RepairTask.h"
#include "task/RetreatTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "UnitDef.h"
#include "Unit.h"

namespace circuit {

using namespace springai;

CBRepairTask::CBRepairTask(ITaskManager* mgr, Priority priority, CCircuitUnit* target, int timeout) :
		IBuilderTask(mgr, priority, nullptr, -RgtVector, BuildType::REPAIR, 1000.0f, timeout)
{
	SetTarget(target);
}

CBRepairTask::~CBRepairTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBRepairTask::Execute(CCircuitUnit* unit)
{
	if (target == nullptr) {
		target = FindUnitToAssist(unit);
		if (target == nullptr) {
			manager->FallbackTask(unit);
			return;
		}
		cost = target->GetDef()->GetCost(manager->GetCircuit()->GetEconomyManager()->GetMetalRes());
	}

	Unit* u = unit->GetUnit();
	std::vector<float> params;
	params.push_back(static_cast<float>(priority));
	u->ExecuteCustomCommand(CMD_PRIORITY, params);

	u->Repair(target->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
}

void CBRepairTask::Update()
{
	// FIXME: Replace const 1000.0f with build time?
	if (cost < 1000.0f) {
		return;
	}
	float currentIncome = manager->GetCircuit()->GetEconomyManager()->GetAvgMetalIncome();
	if (currentIncome < savedIncome * 0.6) {
		manager->AbortTask(this);
	}
}

void CBRepairTask::Cancel()
{
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

void CBRepairTask::OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	Unit* u = unit->GetUnit();
	// TODO: floating retreat coefficient
	if (u->GetHealth() >= u->GetMaxHealth() * 0.8) {
		return;
	}

	manager->AssignTask(unit, manager->GetRetreatTask());
}

void CBRepairTask::SetTarget(CCircuitUnit* unit)
{
	target = unit;
	if (unit != nullptr) {
		cost = unit->GetDef()->GetCost(manager->GetCircuit()->GetEconomyManager()->GetMetalRes());
		position = buildPos = unit->GetUnit()->GetPos();
	} else {
		cost = 1000.0f;
		position = buildPos = -RgtVector;
	}
}

CCircuitUnit* CBRepairTask::FindUnitToAssist(CCircuitUnit* unit)
{
	CCircuitUnit* target = nullptr;
	Unit* su = unit->GetUnit();
	const AIFloat3& pos = su->GetPos();
	float maxSpeed = su->GetMaxSpeed();
	float radius = unit->GetDef()->GetBuildDistance() + maxSpeed * FRAMES_PER_SEC * 30;
	CCircuitAI* circuit = manager->GetCircuit();

	circuit->UpdateAllyUnits();
	std::vector<Unit*> units = circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius);
	for (auto u : units) {
		if (u->GetHealth() < u->GetMaxHealth() && u->GetVel().Length() <= maxSpeed * 1.5) {
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
