/*
 * GuardTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/GuardTask.h"
#include "task/TaskManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"

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
}

bool CFGuardTask::CanAssignTo(CCircuitUnit* unit) const
{
	return (attackPower < maxPower) && unit->GetCircuitDef()->IsRoleRiot();
}

void CFGuardTask::Start(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Guard(vip->GetUnit());
			unit->CmdWantedSpeed(NO_SPEED_LIMIT);
		)
	} else {
		manager->AbortTask(this);
	}
}

void CFGuardTask::Update()
{
	++updCount;

	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip == nullptr) {
		manager->AbortTask(this);
		return;
	}

	CEnemyInfo* target = nullptr;
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = vip->GetPos(frame);
	std::vector<ICoreUnit::Id> enemyIds = circuit->GetCallback()->GetEnemyUnitIdsIn(pos, vip->GetCircuitDef()->GetLosRadius() + 100.f);
	for (ICoreUnit::Id enemyId : enemyIds) {
		CEnemyInfo* ei = circuit->GetEnemyInfo(enemyId);
		if (ei != nullptr) {
			target = ei;
			break;
		}
	}

	if (target != nullptr) {
		for (CCircuitUnit* unit : units) {
			TRY_UNIT(circuit, unit,
				unit->Attack(target, frame + FRAMES_PER_SEC * 60);
			)
		}
	} else {
		for (CCircuitUnit* unit : units) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Guard(vip->GetUnit());
			)
		}
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
