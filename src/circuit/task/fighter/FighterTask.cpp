/*
 * FighterTask.cpp
 *
 *  Created on: Aug 31, 2015
 *      Author: rlcevg
 */

#include "task/fighter/FighterTask.h"
#include "task/TaskManager.h"
#include "task/RetreatTask.h"
#include "terrain/ThreatMap.h"
#include "unit/action/DGunAction.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"

namespace circuit {

using namespace springai;

IFighterTask::IFighterTask(ITaskManager* mgr, FightType type)
		: IUnitTask(mgr, Priority::NORMAL, Type::FIGHTER)
		, fightType(type)
		, position(-RgtVector)
		, attackPower(.0f)
		, target(nullptr)
{
}

IFighterTask::~IFighterTask()
{
}

void IFighterTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	attackPower += unit->GetCircuitDef()->GetPower();

	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->GetDGunMount() != nullptr) {
		CDGunAction* act = new CDGunAction(unit, cdef->GetDGunRange() * 0.9f);
		unit->PushBack(act);
	}
}

void IFighterTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);

	attackPower -= unit->GetCircuitDef()->GetPower();
}

void IFighterTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	for (CCircuitUnit* unit : units) {
		unit->Update(circuit);
	}
}

void IFighterTask::OnUnitIdle(CCircuitUnit* unit)
{
	// TODO: Wait for others if goal reached? Or we stuck far away?

	if (unit->IsRetreat()) {
		manager->AssignTask(unit, manager->GetRetreatTask());
	}
}

void IFighterTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	Unit* u = unit->GetUnit();
	// TODO: floating retreat coefficient
	if (u->GetHealth() > u->GetMaxHealth() * 0.6f) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const float range = unit->GetCircuitDef()->GetMaxRange();
	if ((target != nullptr) && target->IsInLOS() &&
		(target->GetPos().SqDistance2D(unit->GetPos(circuit->GetLastFrame())) < range * range) &&
		(target->GetThreat() < threatMap->GetUnitThreat(unit)))
	{
		unit->SetRetreat(true);
	} else {
		manager->AssignTask(unit, manager->GetRetreatTask());
	}
}

void IFighterTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	RemoveAssignee(unit);
}

} // namespace circuit
