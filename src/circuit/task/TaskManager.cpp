/*
 * TaskManager.cpp
 *
 *  Created on: Feb 1, 2015
 *      Author: rlcevg
 */

#include "task/TaskManager.h"
#include "task/NullTask.h"
#include "task/IdleTask.h"
#include "task/RetreatTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

ITaskManager::ITaskManager()
{
	nullTask = new CNullTask(this);
	idleTask = new CIdleTask(this);
	retreatTask = new CRetreatTask(this);
}

ITaskManager::~ITaskManager()
{
	delete nullTask;
	delete idleTask;
	delete retreatTask;
}

void ITaskManager::AssignTask(CCircuitUnit* unit, IUnitTask* task)
{
	unit->GetTask()->RemoveAssignee(unit);
	task->AssignTo(unit);
	task->Execute(unit);
}

} // namespace circuit
