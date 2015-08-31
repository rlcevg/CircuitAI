/*
 * DefendTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/DefendTask.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CDefendTask::CDefendTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::DEFEND)
{
}

CDefendTask::~CDefendTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CDefendTask::Execute(CCircuitUnit* unit)
{
}

} // namespace circuit
