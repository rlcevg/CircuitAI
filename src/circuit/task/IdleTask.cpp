/*
 * IdleTask.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "task/IdleTask.h"
#include "task/action/AssignAction.h"
#include "util/utils.h"

namespace circuit {

CIdleTask::CIdleTask() :
		IUnitTask(Priority::NORMAL)
{
	PushBack(new CAssignAction(this));
}

CIdleTask::~CIdleTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CIdleTask::RemoveAssignee(CCircuitUnit* unit)
{
	units.erase(unit);
}

void CIdleTask::MarkCompleted()
{
	units.clear();
}

void CIdleTask::OnUnitIdle(CCircuitUnit* unit)
{
	// Do nothing. Unit is already idling.
}

void CIdleTask::OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	// TODO: React while idling: analyze situation and create appropriate task/action
}

void CIdleTask::OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{

}

} // namespace circuit
