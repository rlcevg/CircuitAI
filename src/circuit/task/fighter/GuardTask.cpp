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

CGuardTask::CGuardTask(ITaskManager* mgr, CCircuitUnit* vip, float maxPower)
		: IFighterTask(mgr, FightType::GUARD)
		, vipId(vip->GetId())
		, maxPower(maxPower)
{
}

CGuardTask::~CGuardTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CGuardTask::CanAssignTo(CCircuitUnit* unit) const
{
	return (attackPower < maxPower) && unit->GetCircuitDef()->IsRoleRiot();
}

void CGuardTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Guard(vip->GetUnit());
			unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);
		)
	} else {
		manager->AbortTask(this);
	}
}

void CGuardTask::OnUnitIdle(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip == nullptr) {
		manager->AbortTask(this);
	} else {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Guard(vip->GetUnit());
		)
	}
}

} // namespace circuit
