/*
 * RaidTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/RaidTask.h"
#include "task/TaskManager.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "terrain/PathFinder.h"
#include "unit/action/MoveAction.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CRaidTask::CRaidTask(ITaskManager* mgr)
		: ISquadTask(mgr, FightType::RAID)
{
	CCircuitAI* circuit = manager->GetCircuit();
	float x = rand() % (circuit->GetTerrainManager()->GetTerrainWidth() + 1);
	float z = rand() % (circuit->GetTerrainManager()->GetTerrainHeight() + 1);
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
}

CRaidTask::~CRaidTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CRaidTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (!unit->GetCircuitDef()->IsRoleRaider() ||
		(unit->GetCircuitDef() != leader->GetCircuitDef()))
	{
		return false;
	}
	CCircuitAI* circuit = manager->GetCircuit();
	if (units.size() > circuit->GetMilitaryManager()->GetMaxRaiders()) {
		return false;
	}
	int frame = circuit->GetLastFrame();
	if (leader->GetPos(frame).SqDistance2D(unit->GetPos(frame)) > SQUARE(1000.f)) {
		return false;
	}
	return true;
}

void CRaidTask::AssignTo(CCircuitUnit* unit)
{
	ISquadTask::AssignTo(unit);
	highestRange = std::max(highestRange, unit->GetCircuitDef()->GetLosRadius());

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	CMoveAction* moveAction = new CMoveAction(unit, squareSize);
	unit->PushBack(moveAction);
	moveAction->SetActive(false);
}

void CRaidTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);
	if (leader == nullptr) {
		manager->AbortTask(this);
	} else {
		highestRange = std::max(highestRange, leader->GetCircuitDef()->GetLosRadius());
	}
}

void CRaidTask::Execute(CCircuitUnit* unit)
{
	if (isRegroup || isAttack) {
		return;
	}
	if (!pPath->empty()) {
		CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
		moveAction->SetPath(pPath);
		moveAction->SetActive(true);
	}
}

void CRaidTask::Update()
{
	++updCount;

	/*
	 * Merge tasks if possible
	 */
	if (updCount % 32 == 1) {
		ISquadTask* task = GetMergeTask();
		if (task != nullptr) {
			task->Merge(this);
			units.clear();
			manager->AbortTask(this);
			return;
		}
	}

	/*
	 * Regroup if required
	 */
	bool wasRegroup = isRegroup;
	bool mustRegroup = IsMustRegroup();
	if (isRegroup) {
		if (mustRegroup) {
			CCircuitAI* circuit = manager->GetCircuit();
			int frame = circuit->GetLastFrame() + FRAMES_PER_SEC * 60;
			for (CCircuitUnit* unit : units) {
				const AIFloat3& pos = utils::get_radial_pos(groupPos, SQUARE_SIZE * 8);
				TRY_UNIT(circuit, unit,
					unit->GetUnit()->MoveTo(groupPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame);
					unit->GetUnit()->PatrolTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, frame);
				)

				CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
				moveAction->SetActive(false);
			}
		}
		return;
	}

	bool isExecute = (updCount % 4 == 2);
	if (!isExecute) {
		for (CCircuitUnit* unit : units) {
			isExecute |= unit->IsForceExecute();
		}
		if (!isExecute) {
			if (wasRegroup && !pPath->empty()) {
				for (CCircuitUnit* unit : units) {
					CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
					moveAction->SetPath(pPath);
					moveAction->SetActive(true);
				}
			}
			return;
		}
	}

	/*
	 * Update target
	 */
	FindTarget();

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	isAttack = false;
	if (target != nullptr) {
		isAttack = true;
		position = target->GetPos();
		if (leader->GetCircuitDef()->IsAbleToFly()) {
			if (target->GetUnit()->IsCloaked()) {
				for (CCircuitUnit* unit : units) {
					const AIFloat3& pos = target->GetPos();
					TRY_UNIT(circuit, unit,
						unit->GetUnit()->ExecuteCustomCommand(CMD_ATTACK_GROUND, {pos.x, pos.y, pos.z}, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
					)

					CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
					moveAction->SetActive(false);
				}
			} else {
				for (CCircuitUnit* unit : units) {
					TRY_UNIT(circuit, unit,
						unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
						unit->GetUnit()->ExecuteCustomCommand(CMD_UNIT_SET_TARGET, {(float)target->GetId()});
					)

					CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
					moveAction->SetActive(false);
				}
			}
		} else {
			for (CCircuitUnit* unit : units) {
				const AIFloat3& pos = utils::get_radial_pos(target->GetPos(), SQUARE_SIZE * 8);
				unit->Attack(pos, target, frame + FRAMES_PER_SEC * 60);

				CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
				moveAction->SetActive(false);
			}
		}
		return;
	} else if (!pPath->empty()) {
		position = pPath->back();
		for (CCircuitUnit* unit : units) {
			CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
			moveAction->SetPath(pPath);
			moveAction->SetActive(true);
		}
		return;
	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = leader->GetPos(frame);
	const AIFloat3& threatPos = static_cast<CMoveAction*>(leader->End())->IsActive() ? position : pos;
	if (attackPower <= threatMap->GetThreatAt(leader, threatPos)) {
		position = circuit->GetMilitaryManager()->GetScoutPosition(leader);
	}

	if (utils::is_valid(position) && terrainManager->CanMoveToPos(leader->GetArea(), position)) {
		AIFloat3 startPos = pos;
		AIFloat3 endPos = position;

		CPathFinder* pathfinder = circuit->GetPathfinder();
		pathfinder->SetMapData(leader, threatMap, frame);
		pathfinder->MakePath(*pPath, startPos, endPos, pathfinder->GetSquareSize());

		if (!pPath->empty()) {
//			position = path.back();
			for (CCircuitUnit* unit : units) {
				CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
				moveAction->SetPath(pPath);
				moveAction->SetActive(true);
			}
			return;
		}
	}

	float x = rand() % (terrainManager->GetTerrainWidth() + 1);
	float z = rand() % (terrainManager->GetTerrainHeight() + 1);
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	for (CCircuitUnit* unit : units) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		)
		CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
		moveAction->SetActive(false);
	}
}

void CRaidTask::OnUnitIdle(CCircuitUnit* unit)
{
	ISquadTask::OnUnitIdle(unit);
	if (units.empty()) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	const float maxDist = std::max<float>(lowestRange, circuit->GetPathfinder()->GetSquareSize());
	if (position.SqDistance2D(leader->GetPos(circuit->GetLastFrame())) < SQUARE(maxDist)) {
		float x = rand() % (circuit->GetTerrainManager()->GetTerrainWidth() + 1);
		float z = rand() % (circuit->GetTerrainManager()->GetTerrainHeight() + 1);
		position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	}

	if (units.find(unit) != units.end()) {
		Execute(unit);  // NOTE: Not sure if it has effect
	}
}

void CRaidTask::FindTarget()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	STerrainMapArea* area = leader->GetArea();
	CCircuitDef* cdef = leader->GetCircuitDef();
	const AIFloat3& pos = leader->GetPos(circuit->GetLastFrame());
	const float speed = SQUARE(highestSpeed);
	const float power = attackPower * 0.5f;
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float range = std::max(leader->GetUnit()->GetMaxRange() + threatMap->GetSquareSize() * 2,
								 cdef->GetLosRadius());
	float minSqDist = SQUARE(range);
	float maxThreat = .0f;

	SetTarget(nullptr);  // make adequate enemy->GetTasks().size()
	CEnemyUnit* bestTarget = nullptr;
	CEnemyUnit* mediumTarget = nullptr;
	CEnemyUnit* worstTarget = nullptr;
	static F3Vec enemyPositions;  // NOTE: micro-opt
	threatMap->SetThreatType(leader);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() || (enemy->GetTasks().size() > 1) ||
			(power <= threatMap->GetThreatAt(enemy->GetPos())) ||
			!terrainManager->CanMoveToPos(area, enemy->GetPos()) ||
			(!cdef->HasAntiWater() && (enemy->GetPos().y < -SQUARE_SIZE * 5)) ||
			(enemy->GetUnit()->GetVel().SqLength2D() > speed))
		{
			continue;
		}

		int targetCat;
		float defPower;
		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
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
		}
		if (sqDist < SQUARE(2000.f)) {  // maxSqDist
			enemyPositions.push_back(enemy->GetPos());
		}
	}
	if (bestTarget == nullptr) {
		bestTarget = (mediumTarget != nullptr) ? mediumTarget : worstTarget;
	}

	pPath->clear();
	if (bestTarget != nullptr) {
		SetTarget(bestTarget);
		enemyPositions.clear();
		return;
	}
	if (enemyPositions.empty()) {
		return;
	}

	AIFloat3 startPos = pos;
	circuit->GetPathfinder()->SetMapData(leader, threatMap, circuit->GetLastFrame());
	circuit->GetPathfinder()->FindBestPath(*pPath, startPos, range * 0.5f, enemyPositions);
	enemyPositions.clear();
}

} // namespace circuit
