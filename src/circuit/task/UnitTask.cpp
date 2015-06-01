/*
 * UnitTask.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "task/UnitTask.h"
#include "task/IdleTask.h"
#include "task/TaskManager.h"
#include "unit/CircuitUnit.h"

namespace circuit {

IUnitTask::IUnitTask(ITaskManager* mgr, Priority priority, Type type) :
		manager(mgr),
		priority(priority),
		type(type)
{
}

IUnitTask::~IUnitTask()
{
}

bool IUnitTask::CanAssignTo(CCircuitUnit* unit)
{
	return true;
}

void IUnitTask::AssignTo(CCircuitUnit* unit)
{
	manager->GetIdleTask()->RemoveAssignee(unit);
	unit->SetTask(this);
	units.insert(unit);
}

void IUnitTask::RemoveAssignee(CCircuitUnit* unit)
{
	units.erase(unit);
	manager->GetIdleTask()->AssignTo(unit);
}

void IUnitTask::Close(bool done)
{
	if (done) {
		Finish();
	} else {
		Cancel();
	}

	CIdleTask* idleTask = manager->GetIdleTask();
	for (auto unit : units) {
		idleTask->AssignTo(unit);
	}
	units.clear();
}

void IUnitTask::Finish()
{
}

void IUnitTask::Cancel()
{
}

} // namespace circuit
