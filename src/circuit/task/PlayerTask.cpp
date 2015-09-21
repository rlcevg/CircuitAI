/*
 * PlayerTask.cpp
 *
 *  Created on: Feb 3, 2015
 *      Author: rlcevg
 */

#include "task/PlayerTask.h"
#include "unit/CircuitUnit.h"
#include "util/utils.h"

namespace circuit {

CPlayerTask::CPlayerTask(ITaskManager* mgr)
		: IUnitTask(mgr, Priority::HIGH, Type::PLAYER)
{
}

CPlayerTask::~CPlayerTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CPlayerTask::Execute(CCircuitUnit* unit)
{
}

void CPlayerTask::Update()
{
}

void CPlayerTask::OnUnitIdle(CCircuitUnit* unit)
{
}

void CPlayerTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
}

void CPlayerTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	RemoveAssignee(unit);
	delete this;
}

} // namespace circuit
