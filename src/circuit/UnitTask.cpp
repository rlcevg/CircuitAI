/*
 * UnitTask.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "UnitTask.h"
#include "CircuitUnit.h"

namespace circuit {

IUnitTask::IUnitTask(Priority priority, int difficulty) :
		priority(priority),
		difficulty(difficulty)
{
}

IUnitTask::~IUnitTask()
{
}

void IUnitTask::AssignTo(CCircuitUnit* unit)
{
	difficulty--;
	unit->SetTask(this);
	units.insert(unit);
}

void IUnitTask::RemoveAssignee(CCircuitUnit* unit)
{
	units.erase(unit);
	unit->SetTask(nullptr);
	difficulty++;
}

void IUnitTask::MarkCompleted()
{
	for (auto& unit : units) {
		unit->SetTask(nullptr);
	}
	units.clear();
}

bool IUnitTask::IsFull()
{
	return difficulty <= 0;
}

} // namespace circuit
