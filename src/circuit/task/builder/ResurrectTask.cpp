/*
 * ResurrectTask.cpp
 *
 *  Created on: Apr 16, 2020
 *      Author: rlcevg
 */

#include "task/builder/ResurrectTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"

#include "Feature.h"
#include "FeatureDef.h"

namespace circuit {

using namespace springai;

CResurrectTask::CResurrectTask(ITaskManager* mgr, Priority priority,
							   const AIFloat3& position,
							   float cost, int timeout, float radius)
		: IBuilderTask(mgr, priority, nullptr, position, Type::BUILDER, BuildType::RESURRECT, cost, 0.f, timeout)
		, radius(radius)
{
}

CResurrectTask::~CResurrectTask()
{
}

bool CResurrectTask::CanAssignTo(CCircuitUnit* unit) const
{
	return unit->GetCircuitDef()->IsAbleToResurrect();
}

void CResurrectTask::AssignTo(CCircuitUnit* unit)
{
	IBuilderTask::AssignTo(unit);

	lastTouched = manager->GetCircuit()->GetLastFrame();
}

void CResurrectTask::RemoveAssignee(CCircuitUnit* unit)
{
	IBuilderTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CResurrectTask::Finish()
{
}

void CResurrectTask::Cancel()
{
}

void CResurrectTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	TRY_UNIT(circuit, unit,
		unit->CmdPriority(ClampPriority());
	)

	const int frame = circuit->GetLastFrame();
	AIFloat3 pos;
	float rezzRadius;
	if ((radius == .0f) || !utils::is_valid(position)) {
		pos = circuit->GetTerrainManager()->GetTerrainCenter();
		rezzRadius = pos.Length2D();
	} else {
		pos = position;
		rezzRadius = radius;
	}
	TRY_UNIT(circuit, unit,
		unit->CmdResurrectInArea(pos, rezzRadius, UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
	)
}

bool CResurrectTask::Reevaluate(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (units.empty()) {
		return true;
	}

	/*
	 * Update reclaim position
	 */
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	const std::vector<ICoreUnit::Id>& enemyIds = circuit->GetCallback()->GetEnemyUnitIdsIn(pos, 500.0f);
	for (ICoreUnit::Id enemyId : enemyIds) {
		CEnemyInfo* enemy = circuit->GetEnemyInfo(enemyId);
		if ((enemy != nullptr)
			&& (unit->GetCircuitDef()->IsAttrVampire()
				|| ((enemy->GetCircuitDef() != nullptr)
					&& !enemy->GetCircuitDef()->IsAttacker()
					/* && enemy->GetUnit()->IsBeingBuilt()*/)))
		{
			TRY_UNIT(circuit, unit,
				unit->CmdReclaimEnemy(enemy, UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
			)
			return false;
		}
	}

	COOAICallback* clb = circuit->GetCallback();
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	auto features = circuit->GetCallback()->GetFeaturesIn(pos, 500.0f);
	if (!features.empty()) {
		CTerrainManager* terrainMgr = circuit->GetTerrainManager();
		circuit->GetThreatMap()->SetThreatType(unit);
		float minSqDist = std::numeric_limits<float>::max();
		Resource* metalRes = circuit->GetEconomyManager()->GetMetalRes();
		for (Feature* feature : features) {
			AIFloat3 featPos = feature->GetPosition();
			CTerrainManager::CorrectPosition(featPos);  // Impulsed flying feature
			if (!terrainMgr->CanReachAtSafe2(unit, featPos, unit->GetCircuitDef()->GetBuildDistance())) {
				continue;
			}
			if (!clb->Feature_IsResurrectable(feature->GetFeatureId())) {
				continue;
			}
			FeatureDef* featDef = feature->GetDef();
			float reclaimValue = featDef->GetContainedResource(metalRes)/* * feature->GetReclaimLeft()*/;
			delete featDef;
			if (reclaimValue < 1.0f) {
				continue;
			}
			float sqDist = pos.SqDistance2D(featPos);
			if ((sqDist < minSqDist) && !builderMgr->IsReclaimFeature(featPos, radius)) {
				position = featPos;
				minSqDist = sqDist;
			}
		}
		utils::free_clear(features);
	}

	return true;
}

void CResurrectTask::OnUnitIdle(CCircuitUnit* unit)
{
	manager->AbortTask(this);
}

bool CResurrectTask::IsInRange(const AIFloat3& pos, float range) const
{
	return position.SqDistance2D(pos) <= SQUARE(radius + range);
}

} // namespace circuit
