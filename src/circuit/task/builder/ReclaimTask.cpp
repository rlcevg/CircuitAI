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
#include "terrain/ThreatMap.h"
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
							 float cost, int timeout, float radius, bool isMetal)
		: IBuilderTask(mgr, priority, nullptr, position, BuildType::RECLAIM, cost, false, timeout)
		, radius(radius)
		, isMetal(isMetal)
{
}

CBReclaimTask::~CBReclaimTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CBReclaimTask::CanAssignTo(CCircuitUnit* unit) const
{
	return cost > buildPower * MAX_BUILD_SEC;
}

void CBReclaimTask::AssignTo(CCircuitUnit* unit)
{
	IBuilderTask::AssignTo(unit);

	lastTouched = manager->GetCircuit()->GetLastFrame();
}

void CBReclaimTask::RemoveAssignee(CCircuitUnit* unit)
{
	IBuilderTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CBReclaimTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	Unit* u = unit->GetUnit();
//	TRY_UNIT(circuit, unit,
//		u->ExecuteCustomCommand(CMD_PRIORITY, {ClampPriority()});
//	)

	int frame = circuit->GetLastFrame();
	if (target == nullptr) {
		AIFloat3 pos;
		float reclRadius;
		if ((radius == .0f) || !utils::is_valid(position)) {
			CTerrainManager* terrainManager = circuit->GetTerrainManager();
			float width = terrainManager->GetTerrainWidth() / 2;
			float height = terrainManager->GetTerrainHeight() / 2;
			pos = AIFloat3(width, 0, height);
			reclRadius = sqrtf(width * width + height * height);
		} else {
			pos = position;
			reclRadius = radius;
		}
		TRY_UNIT(circuit, unit,
			u->ReclaimInArea(pos, reclRadius, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
		)
	} else {
		TRY_UNIT(circuit, unit,
			u->ReclaimUnit(target->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
		)
	}
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
		int frame = circuit->GetLastFrame();
		const AIFloat3& pos = unit->GetPos(frame);
		auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(pos, 300.0f));
		if (!enemies.empty()) {
			for (Unit* enemy : enemies) {
				if ((enemy != nullptr) && enemy->IsBeingBuilt()) {
					TRY_UNIT(circuit, unit,
						unit->GetUnit()->ReclaimUnit(enemy, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
					)
					utils::free_clear(enemies);
					return;
				}
			}
			utils::free_clear(enemies);
		}

		auto features = std::move(circuit->GetCallback()->GetFeaturesIn(pos, 500.0f));
		if (!features.empty()) {
			CTerrainManager* terrainManager = circuit->GetTerrainManager();
			circuit->GetThreatMap()->SetThreatType(unit);
			AIFloat3 reclPos;
			float minSqDist = std::numeric_limits<float>::max();
			Resource* metalRes = circuit->GetEconomyManager()->GetMetalRes();
			for (Feature* feature : features) {
				AIFloat3 featPos = feature->GetPosition();
				terrainManager->CorrectPosition(featPos);  // Impulsed flying feature
				if (!terrainManager->CanBuildAt(unit, featPos)) {
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
				const float radius = 8.0f;  // unit->GetCircuitDef()->GetBuildDistance();
				TRY_UNIT(circuit, unit,
					unit->GetUnit()->ReclaimInArea(reclPos, radius, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
				)
			}
			utils::free_clear(features);
		}
	}
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
