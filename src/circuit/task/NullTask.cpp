/*
 * NullTask.cpp
 *
 *  Created on: May 29, 2015
 *      Author: rlcevg
 */

#include "task/NullTask.h"
#include "unit/CircuitUnit.h"
#include "util/utils.h"

namespace circuit {

CNullTask::CNullTask(ITaskManager* mgr)
		: IUnitTask(mgr, IUnitTask::Priority::LOW, IUnitTask::Type::IDLE)
{
}

CNullTask::~CNullTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CNullTask::AssignTo(CCircuitUnit* unit)
{
	unit->SetTask(this);
	units.insert(unit);
}

void CNullTask::Execute(CCircuitUnit* unit)
{
}

void CNullTask::Update()
{
}

void CNullTask::OnUnitIdle(CCircuitUnit* unit)
{
}

void CNullTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
}

void CNullTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	RemoveAssignee(unit);
}

} // namespace circuit
