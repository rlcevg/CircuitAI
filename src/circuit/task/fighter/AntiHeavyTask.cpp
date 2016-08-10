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
#include "unit/action/FightAction.h"
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
	CCircuitDef* cdef = unit->GetCircuitDef();
	highestRange = std::max(highestRange, cdef->GetLosRadius());

	CCircuitAI* circuit = manager->GetCircuit();
	if (cdef->IsAbleToCloak() && !cdef->IsAttrOpenFire()) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->SetFireState(1);
		)
	}

	IUnitAction* act = static_cast<IUnitAction*>(unit->Begin());
	if (act->IsEqual(IUnitAction::Mask::DGUN)) {
		act->SetActive(false);
	}

	int squareSize = circuit->GetPathfinder()->GetSquareSize();
	ITravelAction* travelAction;
	if (cdef->IsAttrSiege()) {
		travelAction = new CFightAction(unit, squareSize);
	} else {
		travelAction = new CMoveAction(unit, squareSize);
	}
	unit->PushBack(travelAction);
	travelAction->SetActive(false);
}

void CAntiHeavyTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);
	if (leader == nullptr) {
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
		ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
		travelAction->SetPath(pPath);
		travelAction->SetActive(true);
	}
}

void CAntiHeavyTask::Update()
{
	++updCount;

	/*
	 * Merge tasks if possible
	 */
	if (updCount % 32 == 1) {
		if (manager->GetCircuit()->GetMilitaryManager()->GetEnemyMetal(CCircuitDef::RoleType::HEAVY) < 1.f) {
			manager->AbortTask(this);
			return;
		}
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

				ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
				travelAction->SetActive(false);
			}
		}
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	bool isExecute = (updCount % 2 == 0) && (frame >= lastTouched + FRAMES_PER_SEC);
	if (!isExecute) {
		for (CCircuitUnit* unit : units) {
			isExecute |= unit->IsForceExecute();
		}
		if (!isExecute) {
			if (wasRegroup && !pPath->empty()) {
				for (CCircuitUnit* unit : units) {
					ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
					travelAction->SetPath(pPath);
					travelAction->SetActive(true);
				}
			}
			return;
		}
	}
	lastTouched = frame;

	/*
	 * Update target
	 */
	if (leader->GetCircuitDef()->IsRoleMine() ||
		(leader->GetCircuitDef()->GetReloadTime() < FRAMES_PER_SEC * 5) ||
		leader->IsWeaponReady(frame))
	{
		FindTarget();

		isAttack = false;
		if (target != nullptr) {
			isAttack = true;
			position = target->GetPos();
			float power = 0.f;
			CEnemyUnit* target = this->target;
			auto subattack = [&power, target](CCircuitUnit* unit) {
				IUnitAction* act = static_cast<IUnitAction*>(unit->Begin());
				if (act->IsEqual(IUnitAction::Mask::DGUN)) {
					act->SetActive(true);
				}
				ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
				travelAction->SetActive(false);

				power += unit->GetCircuitDef()->GetPower();
				return target->GetThreat() < power;
			};
			if (target->GetUnit()->IsCloaked()) {
				const AIFloat3& pos = target->GetPos();
				for (CCircuitUnit* unit : units) {
					unit->Attack(pos, target, frame + FRAMES_PER_SEC * 60);

					if (subattack(unit)) {
						break;
					}
				}
			} else {
				for (CCircuitUnit* unit : units) {
					unit->Attack(target, frame + FRAMES_PER_SEC * 60);

					if (subattack(unit)) {
						break;
					}
				}
			}
			return;
		} else if (!pPath->empty()) {
			position = pPath->back();
			for (CCircuitUnit* unit : units) {
				ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
				travelAction->SetPath(pPath);
				travelAction->SetActive(true);
			}
			return;
		} else {
			CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
			if ((commander != nullptr) &&
				circuit->GetTerrainManager()->CanMoveToPos(leader->GetArea(), commander->GetPos(frame)))
			{
				for (CCircuitUnit* unit : units) {
					unit->Guard(commander, frame + FRAMES_PER_SEC * 60);

					ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
					travelAction->SetActive(false);
				}
				return;
			}
		}
	} else {
		position = circuit->GetSetupManager()->GetBasePos();
		AIFloat3 startPos = leader->GetPos(frame);
		pPath->clear();
		CPathFinder* pathfinder = circuit->GetPathfinder();
		pathfinder->SetMapData(leader, circuit->GetThreatMap(), frame);
		pathfinder->MakePath(*pPath, startPos, position, pathfinder->GetSquareSize() * 4);
	}
	if (pPath->empty()) {  // should never happen
		for (CCircuitUnit* unit : units) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			)

			ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
			travelAction->SetActive(false);
		}
	} else {
		for (CCircuitUnit* unit : units) {
			ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
			travelAction->SetPath(pPath);
			travelAction->SetActive(true);
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

void CAntiHeavyTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	ISquadTask::OnUnitDamaged(unit, attacker);

	if (attacker != nullptr) {
		if (unit->GetCircuitDef()->IsAbleToCloak()) {
			TRY_UNIT(manager->GetCircuit(), unit,
				unit->GetUnit()->SetFireState(2);
			)
		}
		IUnitAction* act = static_cast<IUnitAction*>(unit->Begin());
		if (act->IsEqual(IUnitAction::Mask::DGUN)) {
			act->SetActive(true);
		}
	}
}

void CAntiHeavyTask::FindTarget()
{
	CCircuitAI* circuit = manager->GetCircuit();
	Map* map = circuit->GetMap();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = leader->GetPos(circuit->GetLastFrame());
	STerrainMapArea* area = leader->GetArea();
	CCircuitDef* cdef = leader->GetCircuitDef();
	const int canTargetCat = cdef->GetTargetCategory();
	const float maxPower = attackPower * 8.0f;
	const float weaponRange = cdef->GetMaxRange();
	const float range = std::max(highestRange, threatMap->GetSquareSize() * 2.0f);
	const float losSqDist = SQUARE(range);
	float minSqDist = losSqDist;

	CEnemyUnit* bestTarget = nullptr;
	static F3Vec enemyPositions;  // NOTE: micro-opt
	threatMap->SetThreatType(leader);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden()) {
			continue;
		}
		const AIFloat3& ePos = enemy->GetPos();
		if ((maxPower <= threatMap->GetThreatAt(ePos) - enemy->GetThreat()) ||
			!terrainManager->CanMoveToPos(area, ePos))
		{
			continue;
		}

		CCircuitDef* edef = enemy->GetCircuitDef();
		if ((edef == nullptr) || !edef->IsRoleHeavy() ||
			((edef->GetCategory() & canTargetCat) == 0) ||
			(ePos.y - map->GetElevationAt(ePos.x, ePos.z) > weaponRange))
		{
			continue;
		}

		const float sqDist = pos.SqDistance2D(ePos);
		if (minSqDist > sqDist) {
			minSqDist = sqDist;
			bestTarget = enemy;
		} else if (losSqDist <= sqDist) {
			enemyPositions.push_back(ePos);
		}
	}

	pPath->clear();
	SetTarget(bestTarget);
	if (bestTarget != nullptr) {
		enemyPositions.clear();
		return;
	}
	if (enemyPositions.empty()) {
		return;
	}

	AIFloat3 startPos = pos;
	circuit->GetPathfinder()->SetMapData(leader, threatMap, circuit->GetLastFrame());
	circuit->GetPathfinder()->FindBestPath(*pPath, startPos, threatMap->GetSquareSize(), enemyPositions);
	enemyPositions.clear();
}

} // namespace circuit
