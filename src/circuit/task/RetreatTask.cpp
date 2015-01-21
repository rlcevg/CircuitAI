/*
 * RetreatTask.cpp
 *
 *  Created on: Jan 18, 2015
 *      Author: rlcevg
 */

#include "task/RetreatTask.h"
#include "util/utils.h"

namespace circuit {

CRetreatTask::CRetreatTask() :
		IUnitTask(Priority::NORMAL)
{
}

CRetreatTask::~CRetreatTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CRetreatTask::OnUnitIdle(CCircuitUnit* unit)
{
	// TODO: Do nothing? Check health on update.
}

void CRetreatTask::OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	// TODO: React while idling: analyze situation and create appropriate task
}

void CRetreatTask::OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	// TODO: React while idling: analyze situation and create appropriate task
}

} // namespace circuit
