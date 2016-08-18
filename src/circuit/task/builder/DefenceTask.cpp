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
							 float cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, BuildType::DEFENCE, cost, shake, timeout)
{
}

CBDefenceTask::~CBDefenceTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBDefenceTask::Finish()
{
	CCircuitAI* circuit = manager->GetCircuit();
	// Reclaim turret blockers
	const float radius = 128.0f;  // buildDef->GetMaxRange() * 0.5f;
	auto features = std::move(circuit->GetCallback()->GetFeaturesIn(buildPos, radius));
	if (!features.empty()) {
		circuit->GetBuilderManager()->EnqueueReclaim(IBuilderTask::Priority::HIGH, buildPos, .0f, FRAMES_PER_SEC * 60, radius, false);
		utils::free_clear(features);
	}

	IBuilderTask::Finish();
}

void CBDefenceTask::Cancel()
{
	manager->GetCircuit()->GetMilitaryManager()->AbortDefence(this);

	IBuilderTask::Cancel();
}

} // namespace circuit
