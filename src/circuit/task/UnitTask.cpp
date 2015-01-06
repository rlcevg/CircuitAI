/*
 * UnitTask.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "task/UnitTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

IUnitTask::IUnitTask(Priority priority) :
		priority(priority)
{
}

IUnitTask::~IUnitTask()
{
}

void IUnitTask::AssignTo(CCircuitUnit* unit)
{
	unit->SetTask(this);
	units.insert(unit);
}

void IUnitTask::RemoveAssignee(CCircuitUnit* unit)
{
	units.erase(unit);
	unit->SetTask(nullptr);
}

void IUnitTask::MarkCompleted()
{
	for (auto& unit : units) {
		unit->SetTask(nullptr);
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
