/*
 * AttackTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/AttackTask.h"
#include "task/TaskManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CAttackTask::CAttackTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::ATTACK)
		, isUpdating(false)
		, updCount(0)
{
}

CAttackTask::~CAttackTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CAttackTask::Execute(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();

	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();

	float minSqDist;
	CEnemyUnit* bestTarget = FindBestTarget(unit, minSqDist);

	if (bestTarget == nullptr) {
		if (!isUpdating) {
			float x = rand() % (terrainManager->GetTerrainWidth() + 1);
			float z = rand() % (terrainManager->GetTerrainHeight() + 1);
			position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
		}
	} else {
		position = bestTarget->GetPos();
		float range = u->GetMaxRange();
		if ((bestTarget->GetCircuitDef() != nullptr) && (minSqDist < range * range)) {
			int targetCat = bestTarget->GetCircuitDef()->GetUnitDef()->GetCategory();
			int noChaseCat = unit->GetCircuitDef()->GetUnitDef()->GetNoChaseCategory();
			if (targetCat & noChaseCat != 0) {
				u->Attack(bestTarget->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 300);
				return;
			}
		}
	}
	u->Fight(position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 300);
}

void CAttackTask::Update()
{
	if (updCount++ % 4 == 0) {
		isUpdating = true;
		for (CCircuitUnit* unit : units) {
			Execute(unit);
		}
		isUpdating = false;
	} else {
		IFighterTask::Update();
	}
}

CEnemyUnit* CAttackTask::FindBestTarget(CCircuitUnit* unit, float& minSqDist)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = unit->GetUnit()->GetPos();
	STerrainMapArea* area = unit->GetArea();
	float power = threatMap->GetUnitThreat(unit);

	CEnemyUnit* bestTarget = nullptr;
	minSqDist = std::numeric_limits<float>::max();

	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() || (threatMap->GetThreatAt(enemy->GetPos()) >= power) ||
			!terrainManager->CanMoveToPos(area, enemy->GetPos()))
		{
			continue;
		}
		float sqDist = pos.SqDistance2D(enemy->GetPos());
		if (sqDist < minSqDist) {
			bestTarget = enemy;
			minSqDist = sqDist;
		}
	}

	return bestTarget;
}

} // namespace circuit
