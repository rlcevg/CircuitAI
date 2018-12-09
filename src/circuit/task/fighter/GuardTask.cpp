/*
 * GuardTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/GuardTask.h"
#include "task/TaskManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CFGuardTask::CFGuardTask(ITaskManager* mgr, CCircuitUnit* vip, float maxPower)
		: IFighterTask(mgr, FightType::GUARD, 1.f)
		, vipId(vip->GetId())
		, maxPower(maxPower)
{
}

CFGuardTask::~CFGuardTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CFGuardTask::CanAssignTo(CCircuitUnit* unit) const
{
	return (attackPower < maxPower) && unit->GetCircuitDef()->IsRoleRiot();
}

void CFGuardTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Guard(vip->GetUnit());
			unit->GetUnit()->ExecuteCustomCommand(CMD_WANTED_SPEED, {NO_SPEED_LIMIT});
		)
	} else {
		manager->AbortTask(this);
	}
}

void CFGuardTask::OnUnitIdle(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Guard(vip->GetUnit());
		)
	} else {
		manager->AbortTask(this);
	}
}

} // namespace circuit
