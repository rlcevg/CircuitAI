/*
 * FighterTask.cpp
 *
 *  Created on: Aug 31, 2015
 *      Author: rlcevg
 */

#include "task/fighter/FighterTask.h"
#include "task/TaskManager.h"
#include "task/RetreatTask.h"
#include "unit/action/DGunAction.h"
#include "CircuitAI.h"

namespace circuit {

using namespace springai;

IFighterTask::IFighterTask(ITaskManager* mgr, FightType type)
		: IUnitTask(mgr, Priority::NORMAL, Type::FIGHTER)
		, fightType(type)
		, position(-RgtVector)
{
}

IFighterTask::~IFighterTask()
{
}

void IFighterTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->GetDGunMount() != nullptr) {
		CDGunAction* act = new CDGunAction(unit, cdef->GetDGunRange() * 0.9f);
		unit->PushBack(act);
	}
}

void IFighterTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);

	manager->AbortTask(this);
}

void IFighterTask::Update()
{
	// TODO: Monitor threat? Or do it on EnemySeen/EnemyDestroyed?

	CCircuitAI* circuit = manager->GetCircuit();
	for (CCircuitUnit* unit : units) {
		unit->Update(circuit);
	}
}

void IFighterTask::OnUnitIdle(CCircuitUnit* unit)
{
	// TODO: Wait for others if goal reached? Or we stuck far away?
	manager->AbortTask(this);
}

void IFighterTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	Unit* u = unit->GetUnit();
	// TODO: floating retreat coefficient
	if (u->GetHealth() > u->GetMaxHealth() * 0.6f) {
		return;
	}

	manager->AssignTask(unit, manager->GetRetreatTask());
	manager->AbortTask(this);
}

void IFighterTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
//	RemoveAssignee(unit);
	manager->AbortTask(this);
}

} // namespace circuit
