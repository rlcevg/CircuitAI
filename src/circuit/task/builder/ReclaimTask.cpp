/*
 * ReclaimTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/ReclaimTask.h"
#include "task/TaskManager.h"
#include "map/InfluenceMap.h"
#include "map/ThreatMap.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "unit/action/TravelAction.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"

#include "AISCommands.h"
#include "Feature.h"
#include "FeatureDef.h"

namespace circuit {

using namespace springai;

CBReclaimTask::CBReclaimTask(ITaskManager* mgr, Priority priority,
							 const AIFloat3& position,
							 SResource cost, int timeout, float radius, bool isMetal)
		: IReclaimTask(mgr, priority, Type::BUILDER, position, cost, timeout, radius, isMetal)
{
}

CBReclaimTask::CBReclaimTask(ITaskManager* mgr, Priority priority,
							 CCircuitUnit* target,
							 int timeout)
		: IReclaimTask(mgr, priority, Type::BUILDER, target, timeout)
{
	static_cast<CBuilderManager*>(mgr)->MarkReclaimUnit(target, this);
}

CBReclaimTask::CBReclaimTask(ITaskManager* mgr)
		: IReclaimTask(mgr, Type::BUILDER)
{
}

CBReclaimTask::~CBReclaimTask()
{
}

bool CBReclaimTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (!IReclaimTask::CanAssignTo(unit)) {
		return false;
	}
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const AIFloat3& pos = GetPosition();
	if (unit->IsAttrSolo()
		&& ((circuit->GetInflMap()->GetEnemyInflAt(pos) > INFL_EPS)
			|| (pos.SqDistance2D(unit->GetPos(circuit->GetLastFrame())) > SQUARE(cdef->GetBuildDistance() + SQUARE_SIZE * 8))))
	{
		return false;
	}
	// FIXME: @see CBReclaimTask::Reevaluate, unify checks
	if (isMetal && circuit->GetEconomyManager()->IsMetalFull()) {
		return false;
	}
	// FIXME: @see IBuilderTask::UpdatePath, unify rules at one place.
	//        Unify checks before CanAssignTo() call
	if (!circuit->GetTerrainManager()->CanReachAtSafe(unit, pos, cdef->GetBuildDistance(), cdef->GetPower())) {
		return false;
	}
	CMilitaryManager* militaryMgr = circuit->GetMilitaryManager();
	if ((militaryMgr->GetGuardTaskNum() == 0) || (circuit->GetLastFrame() > militaryMgr->GetGuardFrame())) {
		return true;
	}
	int cluster = circuit->GetMetalManager()->FindNearestCluster(pos);
	if ((cluster < 0) || militaryMgr->HasDefence(cluster)) {
		return true;
	}
	IUnitTask* guard = militaryMgr->GetGuardTask(unit);
	return (guard != nullptr) && !guard->GetAssignees().empty();
}

void CBReclaimTask::AssignTo(CCircuitUnit* unit)
{
	IBuilderTask::AssignTo(unit);

	lastTouched = manager->GetCircuit()->GetLastFrame();
}

bool CBReclaimTask::Reevaluate(CCircuitUnit* unit)
{
	if (!isMetal) {
		return true;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	if (circuit->GetEconomyManager()->IsMetalFull()) {
		manager->AbortTask(this);
		return false;
	} else if (!units.empty()) {
		/*
		 * Update reclaim position
		 */
		const int frame = circuit->GetLastFrame();
		const AIFloat3& pos = unit->GetPos(frame);
		const std::vector<ICoreUnit::Id>& enemyIds = circuit->GetCallback()->GetEnemyUnitIdsIn(pos, 500.0f);
		for (ICoreUnit::Id enemyId : enemyIds) {
			CEnemyInfo* enemy = circuit->GetEnemyInfo(enemyId);
			if (enemy == nullptr) {
				continue;
			}
			CCircuitDef* edef = enemy->GetCircuitDef();
			if ((edef == nullptr) || (edef->IsReclaimable()
				&& (!edef->IsAttacker() || unit->GetCircuitDef()->IsAttrVampire())
				/* && enemy->GetUnit()->IsBeingBuilt()*/))
			{
				TRY_UNIT(circuit, unit,
					unit->CmdReclaimEnemy(enemy, UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
				)
				return false;
			}
		}

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
				FeatureDef* featDef = feature->GetDef();
				if (!featDef->IsReclaimable()) {
					delete featDef;
					continue;
				}
				float reclaimValue = featDef->GetContainedResource(metalRes)/* * feature->GetReclaimLeft()*/;
				delete featDef;
				if (reclaimValue < 1.0f) {
					continue;
				}
				float sqDist = pos.SqDistance2D(featPos);
				if ((sqDist < minSqDist) && !builderMgr->IsResurrect(featPos, radius)) {
					position = featPos;
					minSqDist = sqDist;
				}
			}
			utils::free_clear(features);
		}
	}

	return true;
}

bool CBReclaimTask::Load(std::istream& is)
{
	IReclaimTask::Load(is);

	if (target != nullptr) {
		static_cast<CBuilderManager*>(manager)->MarkReclaimUnit(target, this);
	}
	return true;
}

} // namespace circuit
