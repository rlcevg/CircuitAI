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
#include "resource/MetalManager.h"
#include "unit/EnemyUnit.h"
#include "unit/action/MoveAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CScoutTask::CScoutTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::SCOUT)
		, isUpdating(false)
		, scoutIndex(0)
{
}

CScoutTask::~CScoutTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CScoutTask::AssignTo(CCircuitUnit* unit)
{
	IFighterTask::AssignTo(unit);

	CMoveAction* moveAction = new CMoveAction(unit);
	unit->PushBack(moveAction);
	moveAction->SetActive(false);
}

void CScoutTask::Execute(CCircuitUnit* unit)
{
	IUnitAction* act = static_cast<IUnitAction*>(unit->End());
	if (act->GetType() != IUnitAction::Type::MOVE) {
		return;
	}
	CMoveAction* moveAction = static_cast<CMoveAction*>(act);

	F3Vec path;
	CEnemyUnit* bestTarget = FindBestTarget(unit, path);

	CCircuitAI* circuit = manager->GetCircuit();
	if (bestTarget != nullptr) {
		position = bestTarget->GetPos();
		unit->GetUnit()->Attack(bestTarget->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 300);
		moveAction->SetActive(false);
		return;
	} else if (!path.empty()) {
		position = path.back();
		moveAction->SetPath(path);
		moveAction->SetActive(true);
		unit->Update(circuit);
		return;
	}

	CThreatMap* threatMap = circuit->GetThreatMap();
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Metals& spots = metalManager->GetSpots();
	bool proceed = isUpdating && (threatMap->GetThreatAt(unit, position) < threatMap->GetUnitThreat(unit));
	if (!spots.empty()) {
		if (!proceed) {
			scoutIndex = circuit->GetMilitaryManager()->GetScoutIndex();
		}

		AIFloat3 startPos = unit->GetUnit()->GetPos();
		AIFloat3 endPos = spots[scoutIndex].position;

		CPathFinder* pathfinder = circuit->GetPathfinder();
		pathfinder->SetMapData(unit, threatMap);
		pathfinder->MakePath(path, startPos, endPos, pathfinder->GetSquareSize());

		if (!path.empty()) {
			position = path.back();
			moveAction->SetPath(path);
			moveAction->SetActive(true);
			unit->Update(circuit);
			return;
		}
	}

	if (proceed) {
		return;
	}
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	float x = rand() % (terrainManager->GetTerrainWidth() + 1);
	float z = rand() % (terrainManager->GetTerrainHeight() + 1);
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 300);
	moveAction->SetActive(false);
}

void CScoutTask::Update()
{
	if (updCount++ % 4 == 0) {
		// FIXME: Update group target
		isUpdating = true;
		for (CCircuitUnit* unit : units) {
			Execute(unit);
		}
		isUpdating = false;
	} else {
		IFighterTask::Update();
	}
}

CEnemyUnit* CScoutTask::FindBestTarget(CCircuitUnit* unit, F3Vec& path)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	AIFloat3 pos = unit->GetUnit()->GetPos();
	STerrainMapArea* area = unit->GetArea();
	float power = threatMap->GetUnitThreat(unit) * 0.8f;
	int noChaseCat = unit->GetCircuitDef()->GetUnitDef()->GetNoChaseCategory();
	float range = std::max(unit->GetUnit()->GetMaxRange() + threatMap->GetSquareSize() * 2,
						   unit->GetCircuitDef()->GetUnitDef()->GetLosRadius() * threatMap->GetLosConv());
	float minSqDist = range * range;
	float maxThreat = .0f;

	CEnemyUnit* bestTarget = nullptr;
	CEnemyUnit* mediumTarget = nullptr;
	CEnemyUnit* worstTarget = nullptr;
	F3Vec enemyPositions;
	threatMap->SetThreatType(unit);
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
			if (enemy->GetThreat() > maxThreat) {
				bestTarget = enemy;
				minSqDist = sqDist;
				maxThreat = enemy->GetThreat();
			} else if (bestTarget == nullptr) {
				if (enemy->GetCircuitDef() != nullptr) {
					int targetCat = enemy->GetCircuitDef()->GetUnitDef()->GetCategory();
					if ((targetCat & noChaseCat) == 0) {
						mediumTarget = enemy;
					}
				}
				if (mediumTarget == nullptr) {
					worstTarget = enemy;
				}
			}
		} else {
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

	circuit->GetPathfinder()->SetMapData(unit, threatMap);
	circuit->GetPathfinder()->FindBestPath(path, pos, range * 0.5f, enemyPositions);

	return nullptr;
}

} // namespace circuit
