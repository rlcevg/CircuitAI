/*
 * RepairTask.cpp
 *
 *  Created on: Jan 29, 2015
 *      Author: rlcevg
 */

#include "task/builder/RepairTask.h"
#include "task/RetreatTask.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "unit/action/TravelAction.h"
#include "CircuitAI.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

CBRepairTask::CBRepairTask(IUnitModule* mgr, Priority priority, CAllyUnit* target, int timeout)
		: IRepairTask(mgr, priority, Type::BUILDER, target, timeout)
{
	static_cast<CBuilderManager*>(mgr)->MarkRepairUnit(target->GetId(), this);
}

CBRepairTask::CBRepairTask(IUnitModule* mgr)
		: IRepairTask(mgr, Type::BUILDER)
{
}

CBRepairTask::~CBRepairTask()
{
}

bool CBRepairTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (unit->GetCircuitDef()->IsRoleComm()) {
		return false;
	}
	return IRepairTask::CanAssignTo(unit);
}

void CBRepairTask::Start(CCircuitUnit* unit)
{
	IRepairTask::Start(unit);
	if (targetId != -1) {
		Update(unit);
	}
}

void CBRepairTask::OnUnitIdle(CCircuitUnit* unit)
{
	Unit* u = target->GetUnit();
	if (u->GetHealth() < u->GetMaxHealth()) {
		// unit stuck or event order fail
		RemoveAssignee(unit);
	} else {
		// task finished
		manager->DoneTask(this);
	}
}

void CBRepairTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float healthPerc = unit->GetHealthPercent();
	if ((healthPerc > cdef->GetRetreat()) && !unit->IsDisarmed(frame)) {
		return;
	}

	CRetreatTask* task = manager->EnqueueRetreat();
	manager->AssignTask(unit, task);
}

bool CBRepairTask::Reevaluate(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	// FIXME: Replace const 1000.0f with build time?
	if ((cost.metal > 1000.f)
		&& ((economyMgr->GetAvgMetalIncome() < savedIncome.metal * 0.6f)
			|| (economyMgr->GetAvgEnergyIncome() < savedIncome.energy * 0.6f)))
	{
		manager->AbortTask(this);
		return false;
	}

	CAllyUnit* repTarget = (target != nullptr) ? target : circuit->GetFriendlyUnit(targetId);
	if ((repTarget != nullptr) && (repTarget->GetUnit()->GetHealth() < repTarget->GetUnit()->GetMaxHealth())) {
		buildPos = repTarget->GetPos(circuit->GetLastFrame());
	} else {
		manager->AbortTask(this);
		return false;
	}
	return true;
}

bool CBRepairTask::Load(std::istream& is)
{
	IRepairTask::Load(is);

	CBuilderManager* builderMgr = static_cast<CBuilderManager*>(manager);
	if (target != nullptr) {
		IUnitTask* task = builderMgr->GetRepairTask(target->GetId());
		if (task != nullptr) {
			// FIXME: Units added before Load() by UnitCreated() may create repair task
			builderMgr->AbortTask(task);
		}
		builderMgr->MarkRepairUnit(target->GetId(), this);
	}
	return true;
}

} // namespace circuit
