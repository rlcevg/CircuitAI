/*
 * PlayerTask.cpp
 *
 *  Created on: Feb 3, 2015
 *      Author: rlcevg
 */

#include "task/PlayerTask.h"
#include "unit/CircuitUnit.h"
#include "util/Utils.h"

namespace circuit {

CPlayerTask::CPlayerTask(ITaskManager* mgr)
		: IUnitTask(mgr, Priority::HIGH, Type::PLAYER, -1)
{
}

CPlayerTask::~CPlayerTask()
{
}

void CPlayerTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);
	delete this;
}

void CPlayerTask::Start(CCircuitUnit* unit)
{
}

void CPlayerTask::Update()
{
}

void CPlayerTask::OnUnitIdle(CCircuitUnit* unit)
{
}

void CPlayerTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
}

void CPlayerTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	RemoveAssignee(unit);
}

} // namespace circuit
