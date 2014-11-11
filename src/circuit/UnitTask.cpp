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

void IUnitTask::AssignTo(CCircuitUnit* unit, CCircuitAI* circuit)
{
	unit->SetTask(this);
	units.insert(unit);
}

void IUnitTask::RemoveAssignee(CCircuitUnit* unit)
{
	units.erase(unit);
	unit->SetTask(nullptr);
}

void IUnitTask::MarkCompleted(CCircuitAI* circuit)
{
	if (circuit) {
		circuit->LOG("IUnitTask: %lu", this);
		circuit->LOG("units: %i", units.size());
	}
	int idx = 0;
	for (auto& unit : units) {
		if (circuit) {
			circuit->LOG("idx: %i | unit: %i", idx, unit->GetUnit()->GetUnitId());
			idx++;
		}
		unit->SetTask(nullptr);
	}
	units.clear();
}

std::unordered_set<CCircuitUnit*>& IUnitTask::GetAssignees()
{
	return units;
}

} // namespace circuit
