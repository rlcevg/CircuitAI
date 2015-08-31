/*
 * AttackTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/AttackTask.h"
#include "task/RetreatTask.h"
#include "task/TaskManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "unit/EnemyUnit.h"
#include "unit/action/DGunAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CAttackTask::CAttackTask(ITaskManager* mgr) :
		IUnitTask(mgr, Priority::NORMAL, Type::ATTACK)
{
}

CAttackTask::~CAttackTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CAttackTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->GetDGunMount() != nullptr) {
		CDGunAction* act = new CDGunAction(unit, cdef->GetDGunRange() * 0.9f);
		unit->PushBack(act);
	}
}

void CAttackTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	Unit* u = unit->GetUnit();

	const AIFloat3& pos = u->GetPos();
	STerrainMapArea* area = unit->GetArea();
	float power = circuit->GetThreatMap()->GetUnitPower(unit);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	CEnemyUnit* bestTarget = nullptr;
	float minSqDist = std::numeric_limits<float>::max();
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

	AIFloat3 toPos;
	if (bestTarget == nullptr) {
		float x = rand() % (terrainManager->GetTerrainWidth() + 1);
		float z = rand() % (terrainManager->GetTerrainHeight() + 1);
		toPos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	} else {
		toPos = bestTarget->GetPos();
	}
	u->Fight(toPos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 300);
}

void CAttackTask::Update()
{
	// TODO: Monitor threat? Or do it on EnemySeen/EnemyDestroyed?

	CCircuitAI* circuit = manager->GetCircuit();
	for (CCircuitUnit* unit : units) {
		unit->Update(circuit);
	}
}

void CAttackTask::OnUnitIdle(CCircuitUnit* unit)
{
	// TODO: Wait for others if goal reached? Or we stuck far away?
	manager->AbortTask(this);
}

void CAttackTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	Unit* u = unit->GetUnit();
	// TODO: floating retreat coefficient
	if (u->GetHealth() > u->GetMaxHealth() * 0.6) {
		return;
	}

	manager->AssignTask(unit, manager->GetRetreatTask());
	manager->AbortTask(this);
}

void CAttackTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
//	RemoveAssignee(unit);
	manager->AbortTask(this);
}

} // namespace circuit
