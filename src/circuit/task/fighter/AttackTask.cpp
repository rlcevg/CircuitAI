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
{
}

CAttackTask::~CAttackTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CAttackTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	Unit* u = unit->GetUnit();

	const AIFloat3& pos = u->GetPos();
	STerrainMapArea* area = unit->GetArea();
	float power = circuit->GetThreatMap()->GetUnitThreat(unit);
	CEnemyUnit* bestTarget = nullptr;
	float minSqDist = std::numeric_limits<float>::max();
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() || (enemy->GetThreat() >= power) ||
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

	if (bestTarget == nullptr) {
		float x = rand() % (terrainManager->GetTerrainWidth() + 1);
		float z = rand() % (terrainManager->GetTerrainHeight() + 1);
		position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	} else {
		position = bestTarget->GetPos();
	}
	u->Fight(position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 300);
}

} // namespace circuit
