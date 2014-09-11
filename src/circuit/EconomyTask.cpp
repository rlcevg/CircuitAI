/*
 * EconomyTask.cpp
 *
 *  Created on: Sep 10, 2014
 *      Author: rlcevg
 */

#include "EconomyTask.h"
#include "UnitTask.h"
#include "utils.h"

namespace circuit {

CEconomyTask::CEconomyTask(TaskType type) :
		type(type)
{
}

CEconomyTask::~CEconomyTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CEconomyTask::Execute(CCircuitUnit* unit)
{
	CUnitTask* subtask = subtasks.back();
	return false;
}

} // namespace circuit
