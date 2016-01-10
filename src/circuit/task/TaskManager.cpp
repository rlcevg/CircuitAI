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
		: nullTask(nullptr)
		, idleTask(nullptr)
{
}

ITaskManager::~ITaskManager()
{
	delete nullTask;
	delete idleTask;
}

void ITaskManager::AssignTask(CCircuitUnit* unit, IUnitTask* task)
{
	unit->GetTask()->RemoveAssignee(unit);
	task->AssignTo(unit);
	task->Execute(unit);
}

void ITaskManager::AssignTask(CCircuitUnit* unit)
{
	IUnitTask* task = GetTask(unit);
	if (task != nullptr) {
		task->AssignTo(unit);
	}
}

void ITaskManager::Init()
{
	nullTask = new CNullTask(this);
	idleTask = new CIdleTask(this);
}

} // namespace circuit
