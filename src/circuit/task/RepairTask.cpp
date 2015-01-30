/*
 * RepairTask.cpp
 *
 *  Created on: Jan 29, 2015
 *      Author: rlcevg
 */

#include "task/RepairTask.h"
#include "task/RetreatTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "util/utils.h"

#include "Unit.h"

namespace circuit {

using namespace springai;

CRepairTask::CRepairTask(Priority priority, float cost, CCircuitUnit* target) :
				IUnitTask(priority, Type::REPAIR),
				buildPower(.0f),
				cost(cost),
				target(target)
{
	position = target->GetUnit()->GetPos();
}

CRepairTask::~CRepairTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CRepairTask::Update(CCircuitAI* circuit)
{
	// TODO: Analyze nearby threats? Or threats should update some central system and send messages to all involved?
}

void CRepairTask::OnUnitIdle(CCircuitUnit* unit)
{
	// Task finished or unit stuck (we don't have other signals if its ally)
	unit->GetManager()->AbortTask(this);
	// TODO: Check recent move-fail-position. Maybe it just stuck.
}

void CRepairTask::OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	Unit* u = unit->GetUnit();
	// TODO: floating retreat coefficient
	if (u->GetHealth() > u->GetMaxHealth() * 0.6) {
		return;
	}

	RemoveAssignee(unit);

	unit->GetManager()->GetRetreatTask()->AssignTo(unit);
}

void CRepairTask::OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	unit->GetManager()->AbortTask(this);
}

} // namespace circuit
