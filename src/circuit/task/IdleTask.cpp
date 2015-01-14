/*
 * IdleTask.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "task/IdleTask.h"
#include "unit/action/UnitAction.h"
#include "util/utils.h"

namespace circuit {

CIdleTask* CIdleTask::IdleTask = new CIdleTask;

CIdleTask::CIdleTask() :
		IUnitTask(Priority::NORMAL)
{
//	actionList.PushBack(new CWaitAction(&actionList));
}

CIdleTask::~CIdleTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CIdleTask::CanAssignTo(CCircuitUnit* unit)
{
	return true;
}

} // namespace circuit
