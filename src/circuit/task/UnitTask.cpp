/*
 * UnitTask.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "task/UnitTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "task/IdleTask.h"

namespace circuit {

IUnitTask::IUnitTask(Priority priority) :
		priority(priority)
{
}

IUnitTask::~IUnitTask()
{
}

bool IUnitTask::CanAssignTo(CCircuitUnit* unit)
{
	return true;
}

void IUnitTask::AssignTo(CCircuitUnit* unit)
{
	unit->SetTask(this);
	units.insert(unit);
}

void IUnitTask::RemoveAssignee(CCircuitUnit* unit)
{
	units.erase(unit);
	unit->GetManager()->GetIdleTask()->AssignTo(unit);
}

void IUnitTask::MarkCompleted()
{
	for (auto unit : units) {
		unit->GetManager()->GetIdleTask()->AssignTo(unit);
	}
	units.clear();
}

std::set<CCircuitUnit*>& IUnitTask::GetAssignees()
{
	return units;
}

IUnitTask::Priority IUnitTask::GetPriority()
{
	return priority;
}

} // namespace circuit
