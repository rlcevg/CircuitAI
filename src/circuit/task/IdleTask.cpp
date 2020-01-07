/*
 * IdleTask.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "task/IdleTask.h"
#include "task/TaskManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

CIdleTask::CIdleTask(ITaskManager* mgr)
		: IUnitTask(mgr, Priority::NORMAL, Type::IDLE, -1)
		, updateSlice(0)
{
}

CIdleTask::~CIdleTask()
{
}

void CIdleTask::AssignTo(CCircuitUnit* unit)
{
	unit->SetTask(this);
	units.insert(unit);
}

void CIdleTask::RemoveAssignee(CCircuitUnit* unit)
{
	if (units.erase(unit) > 0) {
		updateUnits.erase(unit);
	}

	unit->ClearAct();
}

void CIdleTask::Start(CCircuitUnit* unit)
{
}

void CIdleTask::Update()
{
	if (updateUnits.empty()) {
		updateUnits = units;  // copy units
		updateSlice = updateUnits.size() / TEAM_SLOWUPDATE_RATE;
	}

	auto it = updateUnits.begin();
	unsigned int i = 0;
	while (it != updateUnits.end()) {
		CCircuitUnit* ass = *it;
		it = updateUnits.erase(it);

		manager->AssignTask(ass);  // should RemoveAssignee() on AssignTo()
		ass->GetTask()->Start(ass);

		if (++i >= updateSlice) {
			break;
		}
	}
}

void CIdleTask::Stop(bool done)
{
	units.clear();
	updateUnits.clear();
}

void CIdleTask::OnUnitIdle(CCircuitUnit* unit)
{
	// Do nothing. Unit is already idling.
}

void CIdleTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	// TODO: React while idling: analyze situation and create appropriate task/action
}

void CIdleTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
}

} // namespace circuit
