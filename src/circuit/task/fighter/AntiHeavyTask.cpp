/*
 * AntiHeavyTask.cpp
 *
 *  Created on: Jun 30, 2016
 *      Author: rlcevg
 */

#include "task/fighter/AntiHeavyTask.h"
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

CAntiHeavyTask::CAntiHeavyTask(ITaskManager* mgr)
		: ISquadTask(mgr, FightType::AH)
{
	CCircuitAI* circuit = manager->GetCircuit();
	float x = rand() % (circuit->GetTerrainManager()->GetTerrainWidth() + 1);
	float z = rand() % (circuit->GetTerrainManager()->GetTerrainHeight() + 1);
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
}

CAntiHeavyTask::~CAntiHeavyTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CAntiHeavyTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (!unit->GetCircuitDef()->IsRoleAH() ||
		(unit->GetCircuitDef() != leader->GetCircuitDef()))
	{
		return false;
	}
	int frame = manager->GetCircuit()->GetLastFrame();
	if (leader->GetPos(frame).SqDistance2D(unit->GetPos(frame)) > SQUARE(1000.f)) {
		return false;
	}
	return true;
}

void CAntiHeavyTask::AssignTo(CCircuitUnit* unit)
{
	ISquadTask::AssignTo(unit);
	highestRange = std::max(highestRange, unit->GetCircuitDef()->GetLosRadius());

	CCircuitAI* circuit = manager->GetCircuit();
	if (unit->GetCircuitDef()->IsAbleToCloak()) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->SetFireState(1);
		)
	}

	int squareSize = circuit->GetPathfinder()->GetSquareSize();
	CMoveAction* moveAction = new CMoveAction(unit, squareSize);
	unit->PushBack(moveAction);
	moveAction->SetActive(false);
}

void CAntiHeavyTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	} else {
		highestRange = std::max(highestRange, leader->GetCircuitDef()->GetLosRadius());
	}

	if (unit->GetCircuitDef()->IsAbleToCloak()) {
		TRY_UNIT(manager->GetCircuit(), unit,
			unit->GetUnit()->SetFireState(2);
		)
	}
}

void CAntiHeavyTask::Execute(CCircuitUnit* unit)
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

void CAntiHeavyTask::Update()
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
				TRY_UNIT(circuit, unit,
					unit->GetUnit()->Fight(groupPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame);
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
		const float sqHighestRange = SQUARE(highestRange);
		for (CCircuitUnit* unit : units) {
			if (position.SqDistance2D(unit->GetPos(frame)) < sqHighestRange) {
				isAttack = true;
				break;
			}
		}
		if (isAttack) {
			for (CCircuitUnit* unit : units) {
				TRY_UNIT(circuit, unit,
					if (unit->IsJumpReady()) {
						const AIFloat3& pos = target->GetPos();
						unit->GetUnit()->ExecuteCustomCommand(CMD_JUMP, {pos.x, pos.y, pos.z}, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
						unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, frame + FRAMES_PER_SEC * 60);
					} else {
						unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
					}
					unit->GetUnit()->ExecuteCustomCommand(CMD_UNIT_SET_TARGET, {(float)target->GetId()});
				)

				CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
				moveAction->SetActive(false);
			}
			return;
		}
	} else {
		CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
		if ((commander != nullptr) &&
			circuit->GetTerrainManager()->CanMoveToPos(leader->GetArea(), commander->GetPos(frame)))
		{
			for (CCircuitUnit* unit : units) {
				unit->Guard(commander, frame + FRAMES_PER_SEC * 60);

				CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
				moveAction->SetActive(false);
			}
			return;
		}
	}
	if (pPath->empty()) {  // should never happen
		for (CCircuitUnit* unit : units) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			)

			CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
			moveAction->SetActive(false);
		}
	} else {
		for (CCircuitUnit* unit : units) {
			CMoveAction* moveAction = static_cast<CMoveAction*>(unit->End());
			moveAction->SetPath(pPath);
			moveAction->SetActive(true);
		}
	}
}

void CAntiHeavyTask::OnUnitIdle(CCircuitUnit* unit)
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

void CAntiHeavyTask::FindTarget()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = leader->GetPos(circuit->GetLastFrame());
	STerrainMapArea* area = leader->GetArea();
	CCircuitDef* cdef = leader->GetCircuitDef();
	const int canTargetCat = cdef->GetTargetCategory();

	CEnemyUnit* bestTarget = nullptr;
	float minSqDist = std::numeric_limits<float>::max();

	std::function<bool (CEnemyUnit* enemy)> badCondition;
	if (circuit->GetMilitaryManager()->GetEnemyMetal(CCircuitDef::RoleType::HEAVY) > .1f) {
		badCondition = [](CEnemyUnit* enemy) {
			return !enemy->GetCircuitDef()->IsRoleHeavy() || enemy->GetUnit()->IsParalyzed();
		};
	} else {
		const int noChaseCat = cdef->GetNoChaseCategory();
		badCondition = [noChaseCat](CEnemyUnit* enemy) {
			return (enemy->GetCircuitDef()->GetCategory() & noChaseCat) != 0;
		};
	}
	threatMap->SetThreatType(leader);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() ||
			(attackPower <= threatMap->GetThreatAt(enemy->GetPos()) - enemy->GetThreat()) ||
			!terrainManager->CanMoveToPos(area, enemy->GetPos()))
		{
			continue;
		}

		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			if (((edef->GetCategory() & canTargetCat) == 0) || badCondition(enemy)) {
				continue;
			}
		}

		const float sqDist = pos.SqDistance2D(enemy->GetPos());
		if (minSqDist > sqDist) {
			minSqDist = sqDist;
			bestTarget = enemy;
		}
	}

	SetTarget(bestTarget);
	if (bestTarget != nullptr) {
		position = target->GetPos();
	}
	AIFloat3 startPos = pos;
	AIFloat3 endPos = position;
	pPath->clear();

	CPathFinder* pathfinder = circuit->GetPathfinder();
	pathfinder->SetMapData(leader, threatMap, circuit->GetLastFrame());
	pathfinder->MakePath(*pPath, startPos, endPos, pathfinder->GetSquareSize());

}

} // namespace circuit
