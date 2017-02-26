/*
 * ArtilleryTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/ArtilleryTask.h"
#include "task/TaskManager.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
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

CArtilleryTask::CArtilleryTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::ARTY, 1.f)
{
	position = manager->GetCircuit()->GetSetupManager()->GetBasePos();
}

CArtilleryTask::~CArtilleryTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CArtilleryTask::CanAssignTo(CCircuitUnit* unit) const
{
	return units.empty() && unit->GetCircuitDef()->IsRoleArty();
}

void CArtilleryTask::AssignTo(CCircuitUnit* unit)
{
	IFighterTask::AssignTo(unit);

	unit->GetUnit()->SetMoveState(0);

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	CMoveAction* travelAction = new CMoveAction(unit, squareSize);
	unit->PushBack(travelAction);
	travelAction->SetActive(false);
}

void CArtilleryTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}

	unit->GetUnit()->SetMoveState(1);
}

void CArtilleryTask::Execute(CCircuitUnit* unit)
{
	Execute(unit, false);
}

void CArtilleryTask::Update()
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

void CArtilleryTask::Execute(CCircuitUnit* unit, bool isUpdating)
{
	IUnitAction* act = static_cast<IUnitAction*>(unit->End());
	if (!act->IsAny(IUnitAction::Mask::MOVE | IUnitAction::Mask::FIGHT | IUnitAction::Mask::JUMP)) {
		return;
	}
	ITravelAction* travelAction = static_cast<ITravelAction*>(act);

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	std::shared_ptr<F3Vec> pPath = std::make_shared<F3Vec>();
	CEnemyUnit* bestTarget = FindTarget(unit, pos, *pPath);

	if (bestTarget != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Attack(bestTarget->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			unit->GetUnit()->ExecuteCustomCommand(CMD_UNIT_SET_TARGET, {(float)bestTarget->GetId()});
		)
		travelAction->SetActive(false);
		return;
	} else if (!pPath->empty()) {
		travelAction->SetPath(pPath);
		travelAction->SetActive(true);
		return;
	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& threatPos = travelAction->IsActive() ? position : pos;
	bool proceed = isUpdating && (threatMap->GetThreatAt(unit, threatPos) < THREAT_MIN);
	if (!proceed) {
		position = circuit->GetMilitaryManager()->GetScoutPosition(unit);
	}

	if (utils::is_valid(position) && terrainManager->CanMoveToPos(unit->GetArea(), position)) {
		AIFloat3 startPos = pos;
		AIFloat3 endPos = position;
//		pPath->clear();

		CPathFinder* pathfinder = circuit->GetPathfinder();
		pathfinder->SetMapData(unit, threatMap, frame);
		pathfinder->MakePath(*pPath, startPos, endPos, pathfinder->GetSquareSize());

		proceed = pPath->size() > 2;
		if (proceed) {
			travelAction->SetPath(pPath);
			travelAction->SetActive(true);
			return;
		}
	}

	if (proceed) {
		return;
	}
	float x = rand() % terrainManager->GetTerrainWidth();
	float z = rand() % terrainManager->GetTerrainHeight();
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	position = terrainManager->GetMovePosition(unit->GetArea(), position);
	TRY_UNIT(circuit, unit,
		unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
	)
	travelAction->SetActive(false);
}

void CArtilleryTask::OnUnitIdle(CCircuitUnit* unit)
{
	IFighterTask::OnUnitIdle(unit);
	if (units.find(unit) != units.end()) {
		RemoveAssignee(unit);
	}
}

CEnemyUnit* CArtilleryTask::FindTarget(CCircuitUnit* unit, const AIFloat3& pos, F3Vec& path)
{
	auto fallback = [this](CCircuitAI* circuit, const AIFloat3& pos, F3Vec& path, CPathFinder* pathfinder) {
		position = circuit->GetSetupManager()->GetBasePos();
		AIFloat3 startPos = pos;
		AIFloat3 endPos = position;
		pathfinder->MakePath(path, startPos, endPos, pathfinder->GetSquareSize());
	};

	CCircuitAI* circuit = manager->GetCircuit();
	CPathFinder* pathfinder = circuit->GetPathfinder();
	CThreatMap* threatMap = circuit->GetThreatMap();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const bool notAW = !cdef->HasAntiWater();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	float range = cdef->GetMaxRange();
	float minSqDist = SQUARE(range);

	static F3Vec enemyPositions;  // NOTE: micro-opt
	threatMap->SetThreatType(unit);
	pathfinder->SetMapData(unit, threatMap, circuit->GetLastFrame());
	bool isPosSafe = (threatMap->GetThreatAt(pos) <= THREAT_MIN);

	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	if (isPosSafe) {
		// Трубка 15, прицел 120, бац, бац …и мимо!
		float maxThreat = .0f;
		CEnemyUnit* bestTarget = nullptr;
		CEnemyUnit* mediumTarget = nullptr;
		CEnemyUnit* worstTarget = nullptr;
		for (auto& kv : enemies) {
			CEnemyUnit* enemy = kv.second;
			if (!enemy->IsInRadarOrLOS() ||
				(notAW && (enemy->GetPos().y < -SQUARE_SIZE * 5)))
			{
				continue;
			}

			CCircuitDef* edef = enemy->GetCircuitDef();
			if ((edef == nullptr) || edef->IsMobile() || edef->IsAttrSiege()) {
				continue;
			}
			int targetCat = edef->GetCategory();
			if ((targetCat & canTargetCat) == 0) {
				continue;
			}

			const float sqDist = pos.SqDistance2D(enemy->GetPos());
			if (sqDist < minSqDist) {
				if (edef->IsRoleBuilder()) {
					bestTarget = enemy;
					minSqDist = sqDist;
					maxThreat = std::numeric_limits<float>::max();
				} else if (edef->GetPower() > maxThreat) {
					bestTarget = enemy;
					minSqDist = sqDist;
					maxThreat = edef->GetPower();
				} else if (bestTarget == nullptr) {
					if ((targetCat & noChaseCat) == 0) {
						mediumTarget = enemy;
					} else if (mediumTarget == nullptr) {
						worstTarget = enemy;
					}
				}
				continue;
			}

			if ((targetCat & noChaseCat) != 0) {
				continue;
			}
//			if (sqDist < SQUARE(2000.f)) {  // maxSqDist
				enemyPositions.push_back(enemy->GetPos());
//			}
		}
		if (bestTarget == nullptr) {
			bestTarget = (mediumTarget != nullptr) ? mediumTarget : worstTarget;
		}
		if (bestTarget != nullptr) {
			position = bestTarget->GetPos();
			return bestTarget;
		}
	} else {
		// Avoid closest units and choose safe position
		for (auto& kv : enemies) {
			CEnemyUnit* enemy = kv.second;
			if (!enemy->IsInRadarOrLOS() ||
				(notAW && (enemy->GetPos().y < -SQUARE_SIZE * 5)))
			{
				continue;
			}

			CCircuitDef* edef = enemy->GetCircuitDef();
			if ((edef == nullptr) || edef->IsMobile()) {
				continue;
			}
			int targetCat = edef->GetCategory();
			if (((targetCat & canTargetCat) == 0) || ((targetCat & noChaseCat) != 0)) {
				continue;
			}

			const float sqDist = pos.SqDistance2D(enemy->GetPos());
			if (sqDist < minSqDist) {
				continue;
			}

//			if (sqDist < SQUARE(2000.f)) {  // maxSqDist
				enemyPositions.push_back(enemy->GetPos());
//			}
		}
	}

	path.clear();
	if (enemyPositions.empty()) {
		return nullptr;
	}

	AIFloat3 startPos = pos;
	range = std::max(range - threatMap->GetSquareSize(), (float)threatMap->GetSquareSize());
	pathfinder->FindBestPath(path, startPos, range, enemyPositions);
	// TODO: Check that path is safe, no threat
	enemyPositions.clear();

	// Check if safe path exists
	if (path.empty()) {
		fallback(circuit, pos, path, pathfinder);
		return nullptr;
	}
	if (threatMap->GetThreatAt(path.back()) > THREAT_MIN) {
		fallback(circuit, pos, path, pathfinder);
	} else {
		position = path.back();
	}

	return nullptr;
}

} // namespace circuit
