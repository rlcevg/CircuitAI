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
#include "unit/action/MoveAction.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CAttackTask::CAttackTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::ATTACK)
		, lowestRange(100000.0f)
		, highestRange(1.0f)
		, lowestSpeed(100000.0f)
		, highestSpeed(1.0f)
		, minPower(.0f)
{
}

CAttackTask::~CAttackTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CAttackTask::CanAssignTo(CCircuitUnit* unit)
{
	return false;
}

void CAttackTask::AssignTo(CCircuitUnit* unit)
{
	IFighterTask::AssignTo(unit);

	lowestRange  = std::min(lowestRange,  unit->GetCircuitDef()->GetMaxRange());
	highestRange = std::max(highestRange, unit->GetCircuitDef()->GetMaxRange());
	lowestSpeed  = std::min(lowestSpeed,  unit->GetCircuitDef()->GetSpeed());
	highestSpeed = std::max(highestSpeed, unit->GetCircuitDef()->GetSpeed());

	minPower += unit->GetCircuitDef()->GetPower() / 4;

//	unit->PushBack(new CMoveAction(unit));
}

void CAttackTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);

	if (attackPower < minPower) {
		manager->AbortTask(this);
	}
}

void CAttackTask::Execute(CCircuitUnit* unit)
{
	Execute(unit, false);
}

void CAttackTask::Update()
{
	int frame = manager->GetCircuit()->GetLastFrame();
	if (++updCount % 16 == 0) {
		AIFloat3 groupPos(ZeroVector);
		for (CCircuitUnit* unit : units) {
			groupPos += unit->GetPos(frame);
		}
		groupPos /= units.size();

		// find the unit closest to the center (since the actual center might be on a hill or something)
		float sqMinDist = std::numeric_limits<float>::max();
		CCircuitUnit* closestUnit = *units.begin();
		for (CCircuitUnit* unit : units) {
			const float sqDist = groupPos.SqDistance2D(unit->GetPos(frame));
			if (sqDist < sqMinDist) {
				sqMinDist = sqDist;
				closestUnit = unit;
			}
		}
		groupPos = closestUnit->GetPos(frame);
	}


	bool isExecute = (++updCount % 4 == 0);
	if (!isExecute) {
		IFighterTask::Update();
		for (CCircuitUnit* unit : units) {
			isExecute |= unit->IsForceExecute();
		}
	}
	if (isExecute) {
		Execute(*units.begin(), true);
		for (CCircuitUnit* unit : units) {
			if (target != nullptr) {
				float range = unit->GetUnit()->GetMaxRange();
				if (position.SqDistance2D(unit->GetPos(frame)) < range * range) {
					unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
					continue;
				}
			}
			unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
			unit->GetUnit()->SetWantedMaxSpeed(lowestSpeed);
		}
	}
}

void CAttackTask::Execute(CCircuitUnit* unit, bool isUpdating)
{
	if ((units.size() > 1) && !isUpdating) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	Unit* u = unit->GetUnit();

	float minSqDist;
	FindTarget(unit, minSqDist);

	if (target == nullptr) {
		if (!isUpdating) {
			float x = rand() % (terrainManager->GetTerrainWidth() + 1);
			float z = rand() % (terrainManager->GetTerrainHeight() + 1);
			position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
		}
	} else {
		position = target->GetPos();
		float range = u->GetMaxRange();
		if (minSqDist < range * range) {
			u->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
			return;
		}
	}
	u->Fight(position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
	u->SetWantedMaxSpeed(lowestSpeed);
}

void CAttackTask::FindTarget(CCircuitUnit* unit, float& minSqDist)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	STerrainMapArea* area = unit->GetArea();
	float power = threatMap->GetUnitThreat(unit);
	int canTargetCat = unit->GetCircuitDef()->GetTargetCategory();

	target = nullptr;
	minSqDist = std::numeric_limits<float>::max();

	threatMap->SetThreatType(unit);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() || (threatMap->GetThreatAt(enemy->GetPos()) >= power) ||
			!terrainManager->CanMoveToPos(area, enemy->GetPos()))
		{
			continue;
		}
		if (((canTargetCat & circuit->GetWaterCategory()) == 0) && (enemy->GetPos().y < -SQUARE_SIZE * 4)) {
			continue;
		}
		CCircuitDef* edef = enemy->GetCircuitDef();
		if ((edef != nullptr) && ((edef->GetCategory() & canTargetCat) == 0)) {
			continue;
		}

		float sqDist = pos.SqDistance2D(enemy->GetPos());
		if (sqDist < minSqDist) {
			target = enemy;
			minSqDist = sqDist;
		}
	}
}

} // namespace circuit
