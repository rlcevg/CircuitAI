/*
 * TaskManager.cpp
 *
 *  Created on: Feb 1, 2015
 *      Author: rlcevg
 */

#include "task/TaskManager.h"
#include "task/IdleTask.h"
#include "task/RetreatTask.h"

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

CIdleTask* ITaskManager::GetIdleTask()
{
	return idleTask;
}

CRetreatTask* ITaskManager::GetRetreatTask()
{
	return retreatTask;
}

} // namespace circuit
