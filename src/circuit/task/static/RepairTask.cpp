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
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"

namespace circuit {

using namespace springai;

CSRepairTask::CSRepairTask(ITaskManager* mgr, Priority priority, CCircuitUnit* target, int timeout)
		: CBRepairTask(mgr, priority, target, timeout)
		, updCount(0)
{
}

CSRepairTask::~CSRepairTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CSRepairTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	float currentIncome = economyManager->GetAvgMetalIncome();
	if ((currentIncome < savedIncome * 0.6) || economyManager->IsMetalFull()) {
		manager->AbortTask(this);
	} else if ((++updCount >= 5) && !units.empty()) {
		updCount = 0;
		CCircuitUnit* target = circuit->GetFriendlyUnit(targetId);
		if ((target != nullptr) && target->GetUnit()->IsBeingBuilt()) {
			/*
			 * Check for damaged units
			 */
			CCircuitUnit* repairTarget = nullptr;
			circuit->UpdateFriendlyUnits();
			float buildDist = (*units.begin())->GetCircuitDef()->GetBuildDistance();
			float sqBuildDist = buildDist * buildDist;
			auto us = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(position, buildDist));
			for (auto u : us) {
				CCircuitUnit* candUnit = circuit->GetFriendlyUnit(u);
				if (candUnit == nullptr) {
					continue;
				}
				if (!u->IsBeingBuilt() && (u->GetHealth() < u->GetMaxHealth()) && (position.SqDistance2D(u->GetPos()) < sqBuildDist)) {
					repairTarget = candUnit;
					break;
				}
			}
			utils::free_clear(us);
			if ((repairTarget != nullptr) && (targetId != repairTarget->GetId())) {
				// Repair task
				IBuilderTask* task = circuit->GetFactoryManager()->EnqueueRepair(IBuilderTask::Priority::NORMAL, repairTarget);
				decltype(units) tmpUnits = units;
				for (auto unit : tmpUnits) {
					manager->AssignTask(unit, task);
				}
				manager->AbortTask(this);
			}
		}
	}
}

void CSRepairTask::OnUnitIdle(CCircuitUnit* unit)
{
	manager->DoneTask(this);
}

} // namespace circuit
