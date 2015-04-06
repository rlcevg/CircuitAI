/*
 * RepairTask.cpp
 *
 *  Created on: Mar 30, 2015
 *      Author: rlcevg
 */

#include "task/static/RepairTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "module/FactoryManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "Unit.h"
#include "UnitDef.h"  // Only for temporary radius

namespace circuit {

using namespace springai;

CSRepairTask::CSRepairTask(ITaskManager* mgr, Priority priority, CCircuitUnit* target, int timeout) :
		CBRepairTask(mgr, priority, target, timeout),
		repUpdCount(0)
{
}

CSRepairTask::~CSRepairTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CSRepairTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	float currentIncome = circuit->GetEconomyManager()->GetAvgMetalIncome();
	if (currentIncome < savedIncome * 0.6) {
		manager->AbortTask(this);
	} else if (++repUpdCount >= 5) {
		repUpdCount = 0;
		/*
		 * Check for damaged units
		 */
		CCircuitUnit* repairTarget = nullptr;
		circuit->UpdateAllyUnits();
		auto us = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(position, (*units.begin())->GetDef()->GetBuildDistance()));
		for (auto u : us) {
			CCircuitUnit* candUnit = circuit->GetFriendlyUnit(u);
			if (candUnit == nullptr) {
				continue;
			}
			if (!u->IsBeingBuilt() && u->GetHealth() < u->GetMaxHealth()) {
				repairTarget = candUnit;
				break;
			}
		}
		utils::free_clear(us);
		if ((repairTarget != nullptr) && (target != repairTarget)) {
			// Repair task
			IBuilderTask* task = circuit->GetFactoryManager()->EnqueueRepair(IBuilderTask::Priority::LOW, repairTarget);
			decltype(units) tmpUnits = units;
			for (auto unit : tmpUnits) {
				manager->AssignTask(unit, task);
			}
			manager->AbortTask(this);
		}
	}
}

void CSRepairTask::OnUnitIdle(CCircuitUnit* unit)
{
	manager->DoneTask(this);
}

} // namespace circuit
