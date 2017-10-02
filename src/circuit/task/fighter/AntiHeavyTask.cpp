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

CAntiHeavyTask::CAntiHeavyTask(ITaskManager* mgr, float powerMod)
		: ISquadTask(mgr, FightType::AH, powerMod)
{
	CCircuitAI* circuit = manager->GetCircuit();
	float x = rand() % circuit->GetTerrainManager()->GetTerrainWidth();
	float z = rand() % circuit->GetTerrainManager()->GetTerrainHeight();
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
	highestRange = std::max(highestRange, cdef->GetJumpRange());

	CCircuitAI* circuit = manager->GetCircuit();
	if (cdef->IsAbleToCloak() && !cdef->IsOpenFire()) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->SetFireState(CCircuitDef::FireType::RETURN);
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
		highestRange = std::max(highestRange, leader->GetCircuitDef()->GetJumpRange());
	}

	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->IsAbleToCloak()) {
		TRY_UNIT(manager->GetCircuit(), unit,
			unit->GetUnit()->SetFireState(cdef->GetFireState());
		)
	}
}

void CAntiHeavyTask::Execute(CCircuitUnit* unit)
{
	if ((State::REGROUP == state) || (State::ENGAGE == state)) {
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
		if (manager->GetCircuit()->GetMilitaryManager()->GetEnemyCost(CCircuitDef::RoleType::HEAVY) < 1.f) {
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
	bool wasRegroup = (State::REGROUP == state);
	bool mustRegroup = IsMustRegroup();
	if (State::REGROUP == state) {
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
	const int frame = circuit->GetLastFrame();
	bool isExecute = (updCount % 2 == 0) && (frame >= lastTouched + FRAMES_PER_SEC);
	if (!isExecute) {
		for (CCircuitUnit* unit : units) {
			isExecute |= unit->IsForceExecute();
		}
		if (!isExecute) {
			if (wasRegroup && !pPath->empty()) {
				ActivePath();
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

		state = State::ROAM;
		if (target != nullptr) {
			state = State::ENGAGE;
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

				if (unit->GetCircuitDef()->IsRoleMine()) {
					const bool isAttack = (target->GetThreat() > power);
					power += unit->GetCircuitDef()->GetPower();
					return isAttack;
				}
				return true;
			};
			if (target->GetUnit()->IsCloaked()) {
				const AIFloat3& pos = target->GetPos();
				for (CCircuitUnit* unit : units) {
					if (subattack(unit)) {
						unit->Attack(pos, target, frame + FRAMES_PER_SEC * 60);
					}
				}
			} else {
				for (CCircuitUnit* unit : units) {
					if (subattack(unit)) {
						unit->Attack(target, frame + FRAMES_PER_SEC * 60);
					}
				}
			}
			return;
		} else if (!pPath->empty()) {
			position = pPath->back();
			ActivePath();
			return;
		}
		if (!leader->GetCircuitDef()->IsRoleMine() &&
			(circuit->GetMilitaryManager()->GetEnemyCost(CCircuitDef::RoleType::HEAVY) < 1.f))
		{
			manager->AbortTask(this);
			return;
		}
	}

	pPath->clear();
	AIFloat3 startPos = leader->GetPos(frame);
	CPathFinder* pathfinder = circuit->GetPathfinder();
	pathfinder->SetMapData(leader, circuit->GetThreatMap(), frame);
	if (leader->GetCircuitDef()->IsRoleMine()) {
		position = circuit->GetSetupManager()->GetBasePos();
		pathfinder->MakePath(*pPath, startPos, position, pathfinder->GetSquareSize() * 4);
	} else {
		circuit->GetMilitaryManager()->FindBestPos(*pPath, startPos, leader->GetArea());
	}

	if (!pPath->empty()) {
		position = pPath->back();
		ActivePath();
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
		position = circuit->GetSetupManager()->GetBasePos();

		for (CCircuitUnit* unit : units) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			)

			ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
			travelAction->SetActive(false);
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
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		float x = rand() % terrainManager->GetTerrainWidth();
		float z = rand() % terrainManager->GetTerrainHeight();
		position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
		position = terrainManager->GetMovePosition(leader->GetArea(), position);
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
				unit->GetUnit()->SetFireState(CCircuitDef::FireType::OPEN);
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
	const bool notAA = !cdef->HasAntiAir();
	const int canTargetCat = cdef->GetTargetCategory();
	const float maxPower = attackPower * powerMod;
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
		if ((edef == nullptr) || !edef->IsEnemyRoleAny(CCircuitDef::RoleMask::HEAVY | CCircuitDef::RoleMask::COMM) ||
			((edef->GetCategory() & canTargetCat) == 0) ||
			(edef->IsAbleToFly() && notAA) ||
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
	circuit->GetPathfinder()->FindBestPath(*pPath, startPos, threatMap->GetSquareSize(), enemyPositions, false);
	enemyPositions.clear();
}

} // namespace circuit
