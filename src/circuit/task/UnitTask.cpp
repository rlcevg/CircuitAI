/*
 * UnitTask.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "task/UnitTask.h"
#include "task/IdleTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"

namespace circuit {

IUnitTask::IUnitTask(Priority priority, Type type) :
		priority(priority),
		type(type)
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
	unit->GetManager()->GetIdleTask()->RemoveAssignee(unit);
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

const std::set<CCircuitUnit*>& IUnitTask::GetAssignees() const
{
	return units;
}

IUnitTask::Priority IUnitTask::GetPriority()
{
	return priority;
}

IUnitTask::Type IUnitTask::GetType()
{
	return type;
}

} // namespace circuit
