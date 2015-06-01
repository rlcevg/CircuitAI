/*
 * ReclaimTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/ReclaimTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "AISCommands.h"
#include "Feature.h"
#include "FeatureDef.h"

namespace circuit {

using namespace springai;

CBReclaimTask::CBReclaimTask(ITaskManager* mgr, Priority priority,
							 const AIFloat3& position,
							 float cost, int timeout, float radius, bool isMetal) :
		IBuilderTask(mgr, priority, nullptr, position, BuildType::RECLAIM, cost, false, timeout),
		radius(radius),
		isMetal(isMetal)
{
}

CBReclaimTask::~CBReclaimTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBReclaimTask::RemoveAssignee(CCircuitUnit* unit)
{
	// Unregister from timeout processor
	manager->SpecialCleanUp(unit);

	IBuilderTask::RemoveAssignee(unit);
}

void CBReclaimTask::Execute(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();

//	std::vector<float> params;
//	params.push_back(static_cast<float>(priority));
//	u->ExecuteCustomCommand(CMD_PRIORITY, params);

	if (target == nullptr) {
		AIFloat3 pos;
		float reclRadius;
		if ((radius == .0f) || (position == -RgtVector)) {
			CTerrainManager* terrain = manager->GetCircuit()->GetTerrainManager();
			float width = terrain->GetTerrainWidth() / 2;
			float height = terrain->GetTerrainHeight() / 2;
			pos = AIFloat3(width, 0, height);
			reclRadius = sqrtf(width * width + height * height);
		} else {
			pos = position;
			reclRadius = radius;
		}
		u->ReclaimInArea(pos, reclRadius, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
	} else {
		u->ReclaimUnit(target->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
	}

	// Register unit to process timeout if set
	manager->SpecialProcess(unit);
}

void CBReclaimTask::Update()
{
	if (!isMetal) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	if (circuit->GetEconomyManager()->IsMetalFull()) {
		manager->AbortTask(this);
	} else if (!units.empty()) {
		/*
		 * Update reclaim position
		 */
		// FIXME: Works only with 1 task per worker
		CCircuitUnit* unit = *units.begin();
		Unit* u = unit->GetUnit();
		const AIFloat3& pos = u->GetPos();
		auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(pos, 300.0f));
		if (!enemies.empty()) {
			for (Unit* enemy : enemies) {
				if (enemy->IsBeingBuilt()) {
					u->ReclaimUnit(enemy, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
					utils::free_clear(enemies);
					return;
				}
			}
			utils::free_clear(enemies);
		}

		auto features = std::move(circuit->GetCallback()->GetFeaturesIn(pos, 500.0f));
		if (!features.empty()) {
			CTerrainManager* terrain = circuit->GetTerrainManager();
			AIFloat3 reclPos;
			float minSqDist = std::numeric_limits<float>::max();
			Resource* metalRes = circuit->GetEconomyManager()->GetMetalRes();
			for (Feature* feature : features) {
				AIFloat3 featPos = feature->GetPosition();
				terrain->CorrectPosition(featPos);  // Impulsed flying feature
				if (!terrain->CanBuildAt(unit, featPos)) {
					continue;
				}
				FeatureDef* featDef = feature->GetDef();
				float reclaimValue = featDef->GetContainedResource(metalRes)/* * feature->GetReclaimLeft()*/;
				delete featDef;
				if (reclaimValue < 1.0f) {
					continue;
				}
				float sqDist = pos.SqDistance2D(featPos);
				if (sqDist < minSqDist) {
					reclPos = featPos;
					minSqDist = sqDist;
				}
			}
			if (minSqDist < std::numeric_limits<float>::max()) {
				u->ReclaimInArea(reclPos, unit->GetCircuitDef()->GetBuildDistance(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			}
			utils::free_clear(features);
		}
	}
}

void CBReclaimTask::Close(bool done)
{
	for (auto unit : units) {
		// Unregister from timeout processor
		manager->SpecialCleanUp(unit);
	}

	IBuilderTask::Close(done);
}

void CBReclaimTask::Finish()
{
}

void CBReclaimTask::Cancel()
{
}

void CBReclaimTask::OnUnitIdle(CCircuitUnit* unit)
{
	manager->AbortTask(this);
}

} // namespace circuit
