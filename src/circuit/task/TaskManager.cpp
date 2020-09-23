/*
 * TaskManager.cpp
 *
 *  Created on: Feb 1, 2015
 *      Author: rlcevg
 */

#include "task/TaskManager.h"
#include "task/NilTask.h"
#include "task/IdleTask.h"
#include "task/PlayerTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

ITaskManager::ITaskManager()
		: nilTask(nullptr)
		, idleTask(nullptr)
		, playerTask(nullptr)
		, metalPull(0.f)
{
}

ITaskManager::~ITaskManager()
{
	delete nilTask;
	delete idleTask;
	delete playerTask;
}

void ITaskManager::AssignTask(CCircuitUnit* unit, IUnitTask* task)
{
	unit->GetTask()->RemoveAssignee(unit);
	task->AssignTo(unit);
	task->Start(unit);
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
	playerTask = new CPlayerTask(this);
}

void ITaskManager::AssignPlayerTask(CCircuitUnit* unit)
{
	AssignTask(unit, playerTask);
}

} // namespace circuit
