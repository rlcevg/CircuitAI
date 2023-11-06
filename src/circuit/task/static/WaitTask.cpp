/*
 * WaitTask.cpp
 *
 *  Created on: May 29, 2017
 *      Author: rlcevg
 */

#include "task/static/WaitTask.h"
#include "util/Utils.h"

namespace circuit {

CSWaitTask::CSWaitTask(IUnitModule* mgr, bool stop, int timeout)
		: IWaitTask(mgr, stop, timeout)
{
}

CSWaitTask::~CSWaitTask()
{
}

void CSWaitTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
}

} // namespace circuit
