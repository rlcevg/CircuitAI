/*
 * IdleTask.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "task/IdleTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

CIdleTask::CIdleTask() :
		IUnitTask(Priority::NORMAL, Type::IDLE)
{
}

CIdleTask::~CIdleTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CIdleTask::AssignTo(CCircuitUnit* unit)
{
	unit->SetTask(this);
	units.insert(unit);
}

void CIdleTask::RemoveAssignee(CCircuitUnit* unit)
{
	units.erase(unit);
}

void CIdleTask::MarkCompleted()
{
	units.clear();
}

void CIdleTask::Update(CCircuitAI* circuit)
{
	auto assignees = units;  // copy assignees
	for (auto ass : assignees) {
		IUnitManager* manager = ass->GetManager();
		manager->AssignTask(ass);  // should RemoveAssignee() on AssignTo()
		manager->ExecuteTask(ass);
		if (!circuit->IsUpdateTimeValid()) {
			break;
		}
	}
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
