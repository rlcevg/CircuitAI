/*
 * ScoutTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/ScoutTask.h"
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
//#include "Drawer.h"

namespace circuit {

using namespace springai;

CScoutTask::CScoutTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::SCOUT)
{
}

CScoutTask::~CScoutTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CScoutTask::CanAssignTo(CCircuitUnit* unit) const
{
	return units.empty() && unit->GetCircuitDef()->IsRoleScout();
}

void CScoutTask::AssignTo(CCircuitUnit* unit)
{
	IFighterTask::AssignTo(unit);

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	CMoveAction* moveAction = new CMoveAction(unit, squareSize);
	unit->PushBack(moveAction);
	moveAction->SetActive(false);
}

void CScoutTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CScoutTask::Execute(CCircuitUnit* unit)
{
	Execute(unit, false);
}

void CScoutTask::Update()
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

void CScoutTask::Execute(CCircuitUnit* unit, bool isUpdating)
{
	IUnitAction* act = static_cast<IUnitAction*>(unit->End());
	if (!act->IsEqual(IUnitAction::Mask::MOVE)) {
		return;
	}
	CMoveAction* moveAction = static_cast<CMoveAction*>(act);

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	std::shared_ptr<F3Vec> pPath = std::make_shared<F3Vec>();
	SetTarget(FindTarget(unit, pos, *pPath));

	if (target != nullptr) {
		position = target->GetPos();
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			unit->GetUnit()->ExecuteCustomCommand(CMD_UNIT_SET_TARGET, {(float)target->GetId()});
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
	bool proceed = isUpdating && (threatMap->GetThreatAt(unit, threatPos) < std::max(threatMap->GetUnitThreat(unit), THREAT_BASE));
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

void CScoutTask::OnUnitIdle(CCircuitUnit* unit)
{
	IFighterTask::OnUnitIdle(unit);
	if (units.find(unit) != units.end()) {
		RemoveAssignee(unit);
	}
}

CEnemyUnit* CScoutTask::FindTarget(CCircuitUnit* unit, const AIFloat3& pos, F3Vec& path)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	STerrainMapArea* area = unit->GetArea();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float speed = cdef->GetSpeed();
	const float power = threatMap->GetUnitThreat(unit) * 0.8f;
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float range = std::max(unit->GetUnit()->GetMaxRange() + threatMap->GetSquareSize() * 2,
								 cdef->GetLosRadius());
	float minSqDist = SQUARE(range);
	float maxThreat = .0f;

	CEnemyUnit* bestTarget = nullptr;
	CEnemyUnit* mediumTarget = nullptr;
	CEnemyUnit* worstTarget = nullptr;
	static F3Vec enemyPositions;  // NOTE: micro-opt
	threatMap->SetThreatType(unit);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() || (enemy->GetTasks().size() > 1) ||
			(power <= threatMap->GetThreatAt(enemy->GetPos())) ||
			!terrainManager->CanMoveToPos(area, enemy->GetPos()) ||
			(!cdef->HasAntiWater() && (enemy->GetPos().y < -SQUARE_SIZE * 5)))
		{
			continue;
		}

		int targetCat;
		float defPower;
		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			if (edef->GetSpeed() > speed) {
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

		float sqDist = pos.SqDistance2D(enemy->GetPos());
		if (enemy->IsInRadarOrLOS() && (sqDist < minSqDist)) {
//			AIFloat3 dir = enemy->GetUnit()->GetPos() - pos;
//			float rayRange = dir.LengthNormalize();
//			CCircuitUnit::Id hitUID = circuit->GetDrawer()->TraceRay(pos, dir, rayRange, u, 0);
//			if (hitUID == enemy->GetId()) {
				if ((defPower > maxThreat) && !enemy->GetUnit()->IsBeingBuilt()) {
					bestTarget = enemy;
					minSqDist = sqDist;
					maxThreat = defPower;
				} else if (bestTarget == nullptr) {
					if ((targetCat & noChaseCat) == 0) {
						mediumTarget = enemy;
					} else if (mediumTarget == nullptr) {
						worstTarget = enemy;
					}
				}
				continue;
//			}
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

	enemyPositions.clear();
	return nullptr;
}

} // namespace circuit
