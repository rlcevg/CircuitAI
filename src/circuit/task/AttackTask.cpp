/*
 * AttackTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/AttackTask.h"
#include "task/RetreatTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "util/utils.h"

#include "Unit.h"

namespace circuit {

using namespace springai;

CAttackTask::CAttackTask() :
		IUnitTask(Priority::NORMAL, Type::ATTACK)
{
}

CAttackTask::~CAttackTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CAttackTask::Update(CCircuitAI* circuit)
{
	// TODO: Monitor threat? Or do it on EnemySeen/EnemyDestroyed?
}

void CAttackTask::OnUnitIdle(CCircuitUnit* unit)
{
	// TODO: Wait for others if goal reached? Or we stuck far away?
}

void CAttackTask::OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	Unit* u = unit->GetUnit();
	if (u->GetHealth() > u->GetMaxHealth() * 0.6) {
		return;
	}

	RemoveAssignee(unit);
	unit->GetManager()->GetRetreatTask()->AssignTo(unit);
}

void CAttackTask::OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	RemoveAssignee(unit);
}

} // namespace circuit
