/*
 * DefenceTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/DefenceTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
#include "module/MilitaryManager.h"
#include "module/EconomyManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "Feature.h"

namespace circuit {

using namespace springai;

CBDefenceTask::CBDefenceTask(ITaskManager* mgr, Priority priority,
							 CCircuitDef* buildDef, const AIFloat3& position,
							 float cost, bool isShake, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, BuildType::DEFENCE, cost, isShake, timeout)
{
}

CBDefenceTask::~CBDefenceTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBDefenceTask::Finish()
{
	CCircuitAI* circuit = manager->GetCircuit();
	float radius = buildDef->GetUnitDef()->GetMaxWeaponRange() * 0.5f;
	auto features = std::move(circuit->GetCallback()->GetFeaturesIn(buildPos, radius));
	if (!features.empty()) {
		IBuilderTask* recl = circuit->GetBuilderManager()->EnqueueReclaim(IBuilderTask::Priority::HIGH, buildPos, .0f, FRAMES_PER_SEC * 60, radius, false);
		if (!units.empty()) {
			manager->AssignTask(*units.begin(), recl);
		}
		utils::free_clear(features);
	}

	IBuilderTask::Finish();
}

void CBDefenceTask::Cancel()
{
	CCircuitAI* circuit = manager->GetCircuit();
	Resource* metalRes = circuit->GetEconomyManager()->GetMetalRes();
	float defCost = buildDef->GetUnitDef()->GetCost(metalRes);
	CMilitaryManager::SDefPoint* point = circuit->GetMilitaryManager()->GetDefPoint(GetPosition(), defCost);
	if (point != nullptr) {
		if ((target == nullptr) && (point->cost >= defCost)) {
			point->cost -= defCost;
		}
		IBuilderTask* next = nextTask;
		while (next != nullptr) {
			defCost = next->GetBuildDef()->GetUnitDef()->GetCost(metalRes);
			if (point->cost >= defCost) {
				point->cost -= defCost;
			}
			next = next->GetNextTask();
		}
	}

	IBuilderTask::Cancel();
}

} // namespace circuit
