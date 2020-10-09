/*
 * ReclaimTask.cpp
 *
 *  Created on: Mar 31, 2015
 *      Author: rlcevg
 */

#include "task/static/ReclaimTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "module/FactoryManager.h"
#include "unit/action/DGunAction.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"

namespace circuit {

using namespace springai;

CSReclaimTask::CSReclaimTask(ITaskManager* mgr, Priority priority,
							 const springai::AIFloat3& position,
							 float cost, int timeout, float radius)
		: IReclaimTask(mgr, priority, Type::FACTORY, position, cost, timeout, radius)
{
}

CSReclaimTask::~CSReclaimTask()
{
}

void CSReclaimTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	ShowAssignee(unit);
	if (!utils::is_valid(position)) {
		position = unit->GetPos(circuit->GetLastFrame());
	}

	if (unit->HasDGun()) {
		unit->PushDGunAct(new CDGunAction(unit, unit->GetDGunRange()));
	}

	lastTouched = circuit->GetLastFrame();
}

void CSReclaimTask::Start(CCircuitUnit* unit)
{
	Execute(unit);
}

void CSReclaimTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (circuit->GetEconomyManager()->IsMetalFull()) {
		manager->AbortTask(this);
	} else if ((++updCount % 4 == 0) && !units.empty()) {
		// Check for damaged units
		CBuilderManager* builderMgr = circuit->GetBuilderManager();
		CAllyUnit* repairTarget = nullptr;
		circuit->UpdateFriendlyUnits();
		auto us = circuit->GetCallback()->GetFriendlyUnitsIn(position, radius * 0.9f);
		for (Unit* u : us) {
			CAllyUnit* candUnit = circuit->GetFriendlyUnit(u);
			if ((candUnit == nullptr) || builderMgr->IsReclaimed(candUnit)
				|| candUnit->GetCircuitDef()->IsMex())  // FIXME: BA, should be IsT1Mex()
			{
				continue;
			}
			if (!u->IsBeingBuilt() && (u->GetHealth() < u->GetMaxHealth())) {
				repairTarget = candUnit;
				break;
			}
		}
		utils::free_clear(us);
		if (repairTarget != nullptr) {
			// Repair task
			IBuilderTask* task = circuit->GetFactoryManager()->EnqueueRepair(IBuilderTask::Priority::NORMAL, repairTarget);
			decltype(units) tmpUnits = units;
			for (CCircuitUnit* unit : tmpUnits) {
				manager->AssignTask(unit, task);
			}
			manager->AbortTask(this);
		}
	}
}

void CSReclaimTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	// TODO: Terraform attacker into dust
}

} // namespace circuit
