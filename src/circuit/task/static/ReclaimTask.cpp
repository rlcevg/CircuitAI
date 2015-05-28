/*
 * ReclaimTask.cpp
 *
 *  Created on: Mar 31, 2015
 *      Author: rlcevg
 */

#include "task/static/ReclaimTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "module/FactoryManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"

namespace circuit {

using namespace springai;

CSReclaimTask::CSReclaimTask(ITaskManager* mgr, Priority priority,
							 const springai::AIFloat3& position,
							 float cost, int timeout, float radius) :
		CBReclaimTask(mgr, priority, position, cost, timeout, radius)
{
}

CSReclaimTask::~CSReclaimTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CSReclaimTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (circuit->GetEconomyManager()->IsMetalFull()) {
		manager->AbortTask(this);
	} else {
		/*
		 * Check for damaged units
		 */
		CCircuitUnit* repairTarget = nullptr;
		circuit->UpdateFriendlyUnits();
		auto us = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(position, radius));
		for (auto u : us) {
			CCircuitUnit* candUnit = circuit->GetFriendlyUnit(u);
			if (candUnit == nullptr) {
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
			IBuilderTask* task = circuit->GetFactoryManager()->EnqueueRepair(IBuilderTask::Priority::LOW, repairTarget);
			decltype(units) tmpUnits = units;
			for (auto unit : tmpUnits) {
				manager->AssignTask(unit, task);
			}
			manager->AbortTask(this);
		}
	}
}

void CSReclaimTask::OnUnitIdle(CCircuitUnit* unit)
{
	manager->DoneTask(this);
}

} // namespace circuit
