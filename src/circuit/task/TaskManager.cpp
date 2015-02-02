/*
 * TaskManager.cpp
 *
 *  Created on: Feb 1, 2015
 *      Author: rlcevg
 */

#include "task/TaskManager.h"
#include "task/IdleTask.h"
#include "task/RetreatTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

ITaskManager::ITaskManager()
{
	idleTask = new CIdleTask(this);
	retreatTask = new CRetreatTask(this);
}

ITaskManager::~ITaskManager()
{
	delete idleTask, retreatTask;
}

void ITaskManager::AssignTask(CCircuitUnit* unit, IUnitTask* task)
{
	unit->GetTask()->RemoveAssignee(unit);
	task->AssignTo(unit);
	task->Execute(unit);
}

CIdleTask* ITaskManager::GetIdleTask()
{
	return idleTask;
}

CRetreatTask* ITaskManager::GetRetreatTask()
{
	return retreatTask;
}

} // namespace circuit
