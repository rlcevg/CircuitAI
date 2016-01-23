/*
 * DefendTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/DefendTask.h"
#include "task/TaskManager.h"
#include "setup/SetupManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CDefendTask::CDefendTask(ITaskManager* mgr, CCircuitUnit* vip, float maxPower)
		: IFighterTask(mgr, FightType::DEFEND)
		, vipId(vip->GetId())
		, maxPower(maxPower)
{
	AIFloat3 offset((float)rand() / RAND_MAX - 0.5f, 0.0f, (float)rand() / RAND_MAX - 0.5f);
	position = manager->GetCircuit()->GetSetupManager()->GetBasePos() + offset.Normalize2D() * SQUARE_SIZE * 32;
}

CDefendTask::~CDefendTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CDefendTask::CanAssignTo(CCircuitUnit* unit)
{
	return (attackPower < maxPower) && unit->GetCircuitDef()->IsRoleRiot();
}

void CDefendTask::Execute(CCircuitUnit* unit)
{
	CCircuitUnit* vip = manager->GetCircuit()->GetTeamUnit(vipId);
	if (vip != nullptr) {
		unit->GetUnit()->Guard(vip->GetUnit());
		unit->GetUnit()->SetWantedMaxSpeed(MAX_SPEED);
	} else {
		manager->AbortTask(this);
	}
}

void CDefendTask::OnUnitIdle(CCircuitUnit* unit)
{
	CCircuitUnit* vip = manager->GetCircuit()->GetTeamUnit(vipId);
	if (vip == nullptr) {
		manager->AbortTask(this);
	} else {
		unit->GetUnit()->Guard(vip->GetUnit());
	}
}

} // namespace circuit
