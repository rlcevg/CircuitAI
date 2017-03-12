/*
 * TaskManager.cpp
 *
 *  Created on: Feb 1, 2015
 *      Author: rlcevg
 */

#include "task/TaskManager.h"
#include "task/NilTask.h"
#include "task/IdleTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

ITaskManager::ITaskManager()
		: nilTask(nullptr)
		, idleTask(nullptr)
		, metalPull(0.f)
{
}

ITaskManager::~ITaskManager()
{
	delete nilTask;
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
	IUnitTask* task = MakeTask(unit);
	if (task != nullptr) {
		task->AssignTo(unit);
	}
}

void ITaskManager::AddMetalPull(CCircuitUnit* unit)
{
	metalPull += unit->GetBuildSpeed();
}

void ITaskManager::DelMetalPull(CCircuitUnit* unit)
{
	metalPull -= unit->GetBuildSpeed();
}

void ITaskManager::Init()
{
	nilTask = new CNilTask(this);
	idleTask = new CIdleTask(this);
}

} // namespace circuit
