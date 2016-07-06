/*
 * BombTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/BombTask.h"
#include "task/TaskManager.h"
#include "module/MilitaryManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "terrain/PathFinder.h"
#include "unit/EnemyUnit.h"
#include "unit/action/MoveAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBombTask::CBombTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::BOMB)
		, isDanger(false)
{
}

CBombTask::~CBombTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CBombTask::CanAssignTo(CCircuitUnit* unit) const
{
	return units.empty() && unit->GetCircuitDef()->IsRoleBomber();
}

void CBombTask::AssignTo(CCircuitUnit* unit)
{
	IFighterTask::AssignTo(unit);

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	CMoveAction* moveAction = new CMoveAction(unit, squareSize);
	unit->PushBack(moveAction);
	moveAction->SetActive(false);
}

void CBombTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CBombTask::Execute(CCircuitUnit* unit)
{
	Execute(unit, false);
}

void CBombTask::Update()
{
	if (++updCount % 4 == 0) {
		for (CCircuitUnit* unit : units) {
			Execute(unit, true);
		}
	} else {
		for (CCircuitUnit* unit : units) {
			if (unit->IsForceExecute()) {
				Execute(unit, true);
			}
		}
	}
}

void CBombTask::Execute(CCircuitUnit* unit, bool isUpdating)
{
	IUnitAction* act = static_cast<IUnitAction*>(unit->End());
	if (!act->IsEqual(IUnitAction::Mask::MOVE)) {
		return;
	}
	CMoveAction* moveAction = static_cast<CMoveAction*>(act);

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	if (!unit->IsWeaponReady(frame)) {  // is unit armed?
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->ExecuteCustomCommand(CMD_FIND_PAD, {}, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		)
		SetTarget(nullptr);
		return;
	}

	if ((target != nullptr) && isDanger) {
		return;
	} else {
		isDanger = false;
	}

	const AIFloat3& pos = unit->GetPos(frame);
	std::shared_ptr<F3Vec> pPath = std::make_shared<F3Vec>();
	SetTarget(FindTarget(unit, pos, *pPath));

	if (target != nullptr) {
		position = target->GetPos();
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		)
		moveAction->SetActive(false);
		return;
	} else if (!pPath->empty()) {
		position = pPath->back();
		moveAction->SetPath(pPath);
		moveAction->SetActive(true);
		return;
	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& threatPos = moveAction->IsActive() ? position : pos;
	bool proceed = isUpdating && (threatMap->GetThreatAt(unit, threatPos) < threatMap->GetUnitThreat(unit));
	if (!proceed) {
		position = circuit->GetMilitaryManager()->GetScoutPosition(unit);
	}

	if (utils::is_valid(position) && terrainManager->CanMoveToPos(unit->GetArea(), position)) {
		AIFloat3 startPos = pos;
		AIFloat3 endPos = position;

		CPathFinder* pathfinder = circuit->GetPathfinder();
		pathfinder->SetMapData(unit, threatMap, frame);
		pathfinder->MakePath(*pPath, startPos, endPos, pathfinder->GetSquareSize());

		if (!pPath->empty()) {
//			position = path.back();
			moveAction->SetPath(pPath);
			moveAction->SetActive(true);
			return;
		}
	}

	if (proceed) {
		return;
	}
	float x = rand() % (terrainManager->GetTerrainWidth() + 1);
	float z = rand() % (terrainManager->GetTerrainHeight() + 1);
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	TRY_UNIT(circuit, unit,
		unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
	)
	moveAction->SetActive(false);
}

void CBombTask::OnUnitIdle(CCircuitUnit* unit)
{
	IFighterTask::OnUnitIdle(unit);
	if (units.find(unit) != units.end()) {
		RemoveAssignee(unit);
	}
}

void CBombTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	if (target == nullptr) {
		IFighterTask::OnUnitDamaged(unit, attacker);
	}

	isDanger = true;
}

CEnemyUnit* CBombTask::FindTarget(CCircuitUnit* unit, const AIFloat3& pos, F3Vec& path)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CThreatMap* threatMap = circuit->GetThreatMap();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float power = threatMap->GetUnitThreat(unit) * 2.0f;
	const float speed = cdef->GetSpeed();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float range = std::max(unit->GetUnit()->GetMaxRange() + threatMap->GetSquareSize() * 2,
								 cdef->GetLosRadius());
	const float sqRange = SQUARE(range);
	float maxThreat = .0f;

	CEnemyUnit* bestTarget = nullptr;
	CEnemyUnit* mediumTarget = nullptr;
	CEnemyUnit* worstTarget = nullptr;
	F3Vec enemyPositions;
	threatMap->SetThreatType(unit);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() ||
			(power <= threatMap->GetThreatAt(enemy->GetPos()) - enemy->GetThreat()) ||
			(!cdef->HasAntiWater() && (enemy->GetPos().y < -SQUARE_SIZE * 5)))
		{
			continue;
		}

		int targetCat;
		float defPower;
		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			if (edef->GetSpeed() * 1.8f > speed) {
				continue;
			}
			targetCat = edef->GetCategory();
			if ((targetCat & canTargetCat) == 0) {
				continue;
			}
			defPower = edef->GetPower();
		} else {
			targetCat = UNKNOWN_CATEGORY;
			defPower = enemy->GetThreat();
		}

		float sumPower = 0.f;
		for (IFighterTask* task : enemy->GetTasks()) {
			sumPower += task->GetAttackPower();
		}
		if (sumPower > defPower) {
			continue;
		}

		float sqDist = pos.SqDistance2D(enemy->GetPos());
		if (enemy->IsInRadarOrLOS() && (sqDist < sqRange)) {
			if (defPower > maxThreat) {
				bestTarget = enemy;
				maxThreat = defPower;
			} else if (bestTarget == nullptr) {
				if ((targetCat & noChaseCat) == 0) {
					mediumTarget = enemy;
				} else if (mediumTarget == nullptr) {
					worstTarget = enemy;
				}
			}
			continue;
		}
		if (sqDist < SQUARE(2000.f)) {  // maxSqDist
			enemyPositions.push_back(enemy->GetPos());
		}
	}
	if (bestTarget == nullptr) {
		bestTarget = (mediumTarget != nullptr) ? mediumTarget : worstTarget;
	}

	path.clear();
	if ((bestTarget != nullptr) || enemyPositions.empty()) {
		return bestTarget;
	}

	AIFloat3 startPos = pos;
	circuit->GetPathfinder()->SetMapData(unit, threatMap, circuit->GetLastFrame());
	circuit->GetPathfinder()->FindBestPath(path, startPos, range * 0.5f, enemyPositions);

	return nullptr;
}

} // namespace circuit
