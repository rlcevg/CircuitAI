/*
 * UnitTask.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "UnitTask.h"
#include "CircuitUnit.h"

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

std::unordered_set<CCircuitUnit*>& IUnitTask::GetAssignees()
{
	return units;
}

IUnitTask::Priority IUnitTask::GetPriority()
{
	return priority;
}

} // namespace circuit
