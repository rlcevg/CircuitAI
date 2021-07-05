/*
 * GuardTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/GuardTask.h"
#include "task/TaskManager.h"
#include "module/MilitaryManager.h"
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
	CCircuitAI* circuit = manager->GetCircuit();
	unsigned int guardsNum = circuit->GetMilitaryManager()->GetGuardsNum();
	if (unit->GetCircuitDef()->IsRoleRiot()) {
		guardsNum /= 2;
	}
	if (units.size() >= guardsNum) {
		return false;
	}
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip == nullptr) {
		return false;
	}
	if (((unit->GetCircuitDef()->IsAmphibious() || unit->GetCircuitDef()->IsSurfer())
			&& (vip->GetCircuitDef()->IsAbleToDive() || vip->GetCircuitDef()->IsSurfer()))
		|| (vip->GetCircuitDef()->IsSubmarine() && unit->GetCircuitDef()->IsSubmarine())
		|| (vip->GetCircuitDef()->IsAbleToFly() && unit->GetCircuitDef()->IsAbleToFly())
		|| (vip->GetCircuitDef()->IsLander() && unit->GetCircuitDef()->IsLander())
		|| (vip->GetCircuitDef()->IsFloater() && unit->GetCircuitDef()->IsFloater()))
	{
		return true;
	}
	return false;
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
//	if (updCount++ % 2 != 0) {
//		return;
//	}

	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip == nullptr) {
		manager->AbortTask(this);
		return;
	}

	CEnemyInfo* target = nullptr;
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = vip->GetPos(frame);
	const std::vector<ICoreUnit::Id>& enemyIds = circuit->GetCallback()->GetEnemyUnitIdsIn(pos, vip->GetCircuitDef()->GetLosRadius() + 500.f);
	for (ICoreUnit::Id enemyId : enemyIds) {
		CEnemyInfo* ei = circuit->GetEnemyInfo(enemyId);
		if (ei != nullptr) {
			target = ei;
			break;
		}
	}

	if (target != nullptr) {
		state = State::ENGAGE;
		const bool isGroundAttack = target->GetUnit()->IsCloaked();
		for (CCircuitUnit* unit : units) {
			unit->Attack(target, isGroundAttack, frame + FRAMES_PER_SEC * 60);
		}
	} else if (State::ENGAGE == state) {
		state = State::ROAM;
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
