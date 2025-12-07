/*
 * WaitTask.cpp
 *
 *  Created on: May 29, 2017
 *      Author: rlcevg
 */

#include "task/static/WaitTask.h"
#include "unit/CircuitUnit.h"
#include "util/Utils.h"

namespace circuit {

CSWaitTask::CSWaitTask(ITaskModule* mgr, bool stop, int timeout)
		: IWaitTask(mgr, stop, timeout)
{
}

CSWaitTask::~CSWaitTask()
{
}

void CSWaitTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	if (unit->GetHealthPercent() < unit->GetCircuitDef()->GetSelfDHP()) {
		unit->CmdSelfD(true);
	}
}

} // namespace circuit
