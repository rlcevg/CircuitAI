/*
 * ScoutTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/ScoutTask.h"
#include "util/utils.h"

namespace circuit {

CScoutTask::CScoutTask(ITaskManager* mgr) :
		IUnitTask(mgr, Priority::NORMAL, Type::SCOUT)
{
}

CScoutTask::~CScoutTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CScoutTask::Execute(CCircuitUnit* unit)
{

}

void CScoutTask::Update()
{
	// TODO: Monitor threat? Or do it on EnemySeen/EnemyDestroyed?
}

void CScoutTask::OnUnitIdle(CCircuitUnit* unit)
{
	// TODO: Go scout elsewhere? Or join the force?
}

void CScoutTask::OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	// TODO: Update threat and try to avoid?
}

void CScoutTask::OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	RemoveAssignee(unit);
}

} // namespace circuit
