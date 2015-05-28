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
	float radius = buildDef->GetUnitDef()->GetMaxWeaponRange() * 0.8;
	auto features = std::move(circuit->GetCallback()->GetFeaturesIn(buildPos, radius));
	if (!features.empty()) {
		IBuilderTask* recl = circuit->GetBuilderManager()->EnqueueReclaim(IBuilderTask::Priority::HIGH, buildPos, .0f, FRAMES_PER_SEC * 60, radius, false);
		if (!units.empty()) {
			manager->AssignTask(*units.begin(), recl);
		}
	}
	utils::free_clear(features);

	IBuilderTask::Finish();
}

void CBDefenceTask::Cancel()
{
	if (target == nullptr) {
		manager->GetCircuit()->GetMilitaryManager()->OpenDefPoint(GetPosition());
	}

	IBuilderTask::Cancel();
}

} // namespace circuit
