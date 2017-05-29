/*
 * WaitTask.cpp
 *
 *  Created on: May 29, 2017
 *      Author: rlcevg
 */

#include "task/static/WaitTask.h"
#include "util/utils.h"

namespace circuit {

CSWaitTask::CSWaitTask(ITaskManager* mgr, int timeout)
		: IWaitTask(mgr, timeout)
{
}

CSWaitTask::~CSWaitTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CSWaitTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
}

} // namespace circuit
