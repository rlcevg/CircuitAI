/*
 * WaitTask.cpp
 *
 *  Created on: Jul 24, 2016
 *      Author: rlcevg
 */

#include "task/common/WaitTask.h"
#include "task/TaskManager.h"
#include "CircuitAI.h"

namespace circuit {

IWaitTask::IWaitTask(ITaskManager* mgr, int timeout)
		: IUnitTask(mgr, Priority::NORMAL, Type::WAIT, timeout)
{
}

IWaitTask::~IWaitTask()
{
}

void IWaitTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	lastTouched = manager->GetCircuit()->GetLastFrame();
}

void IWaitTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);

	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void IWaitTask::Execute(CCircuitUnit* unit)
{
}

void IWaitTask::Update()
{
}

void IWaitTask::OnUnitIdle(CCircuitUnit* unit)
{
}

void IWaitTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	RemoveAssignee(unit);
}

} // namespace circuit
