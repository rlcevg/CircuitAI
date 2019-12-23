/*
 * DefenceTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/DefenceTask.h"
#include "task/TaskManager.h"
#include "map/InfluenceMap.h"
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
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::DEFENCE, cost, shake, timeout)
		, isUrgent(false)
		, normalCost(cost)
{
}

CBDefenceTask::~CBDefenceTask()
{
}

void CBDefenceTask::Update()
{
	IBuilderTask::Update();
	if (units.empty() || (target == nullptr)) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	bool isUnderEnemy = (circuit->GetInflMap()->GetEnemyInflAt(buildPos) > INFL_BASE);
	if (isUrgent == isUnderEnemy) {
		return;
	}
	isUrgent = isUnderEnemy;
	if (isUnderEnemy) {
		priority = IBuilderTask::Priority::HIGH;
		cost *= 8.f;  // attract more builders
	} else {
		priority = IBuilderTask::Priority::NORMAL;
		cost = normalCost;
	}
	TRY_UNIT(circuit, target,
		target->GetUnit()->ExecuteCustomCommand(CMD_PRIORITY, {ClampPriority()});
	)
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
