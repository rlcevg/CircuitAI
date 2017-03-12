/*
 * NilTask.cpp
 *
 *  Created on: May 29, 2015
 *      Author: rlcevg
 */

#include "task/NilTask.h"
#include "unit/CircuitUnit.h"
#include "util/utils.h"

namespace circuit {

CNilTask::CNilTask(ITaskManager* mgr)
		: IUnitTask(mgr, IUnitTask::Priority::LOW, Type::NIL, -1)
{
}

CNilTask::~CNilTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CNilTask::AssignTo(CCircuitUnit* unit)
{
	unit->SetTask(this);
}

void CNilTask::RemoveAssignee(CCircuitUnit* unit)
{
}

void CNilTask::Execute(CCircuitUnit* unit)
{
}

void CNilTask::Update()
{
}

void CNilTask::Close(bool done)
{
}

void CNilTask::OnUnitIdle(CCircuitUnit* unit)
{
}

void CNilTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
}

void CNilTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
}

} // namespace circuit
