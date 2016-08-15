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
	CMoveAction* travelAction = new CMoveAction(unit, squareSize);
	unit->PushBack(travelAction);
	travelAction->SetActive(false);
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
	if (!act->IsAny(IUnitAction::Mask::MOVE | IUnitAction::Mask::FIGHT | IUnitAction::Mask::JUMP)) {
		return;
	}
	ITravelAction* travelAction = static_cast<ITravelAction*>(act);

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
			if (target->GetUnit()->IsCloaked()) {
				unit->GetUnit()->ExecuteCustomCommand(CMD_ATTACK_GROUND, {position.x, position.y, position.z},
													  UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			} else {
				unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			}
		)
		travelAction->SetActive(false);
		return;
	} else if (!pPath->empty()) {
		position = pPath->back();
		travelAction->SetPath(pPath);
		travelAction->SetActive(true);
		return;
	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& threatPos = travelAction->IsActive() ? position : pos;
	bool proceed = isUpdating && (threatMap->GetThreatAt(unit, threatPos) < threatMap->GetUnitThreat(unit));
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
//			position = path.back();
			travelAction->SetPath(pPath);
			travelAction->SetActive(true);
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
	travelAction->SetActive(false);
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
	const float scale = (cdef->GetMinRange() > 400.0f) ? 8.0f : 1.0f;
	const float maxPower = threatMap->GetUnitThreat(unit) * scale;
//	const float maxAltitude = cdef->GetAltitude();
	const float speed = cdef->GetSpeed() / 1.75f;
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float range = std::max(unit->GetUnit()->GetMaxRange() + threatMap->GetSquareSize() * 2,
								 cdef->GetLosRadius());
	float minSqDist = SQUARE(range);
	float maxThreat = .0f;

	CEnemyUnit* bestTarget = nullptr;
	static F3Vec enemyPositions;  // NOTE: micro-opt
	threatMap->SetThreatType(unit);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden()) {
			continue;
		}
		float power = threatMap->GetThreatAt(enemy->GetPos()) - enemy->GetThreat();
		if ((maxPower <= power) ||
			(!cdef->HasAntiWater() && (enemy->GetPos().y < -SQUARE_SIZE * 5)))
		{
			continue;
		}

		int targetCat;
//		float altitude;
		float defPower;
		bool isBuilder;
		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			if (edef->GetSpeed() > speed) {
				continue;
			}
			targetCat = edef->GetCategory();
			if ((targetCat & canTargetCat) == 0) {
				continue;
			}
//			altitude = edef->GetAltitude();
			defPower = edef->GetPower();
			isBuilder = edef->IsRoleBuilder();
		} else {
			targetCat = UNKNOWN_CATEGORY;
//			altitude = 0.f;
			defPower = enemy->GetThreat();
			isBuilder = false;
		}

//		float sumPower = 0.f;
//		for (IFighterTask* task : enemy->GetTasks()) {
//			sumPower += task->GetAttackPower();
//		}
//		if (sumPower > defPower) {
//			continue;
//		}

		float sqDist = pos.SqDistance2D(enemy->GetPos());
		if (minSqDist > sqDist) {
			if (enemy->IsInRadarOrLOS() && ((targetCat & noChaseCat) == 0)/* && (altitude < maxAltitude)*/) {
				if (isBuilder) {
					bestTarget = enemy;
					minSqDist = sqDist;
					maxThreat = std::numeric_limits<float>::max();
				} else if (maxThreat <= defPower) {
					bestTarget = enemy;
					minSqDist = sqDist;
					maxThreat = defPower;
				}
			}
			continue;
		}
		if (sqDist < SQUARE(2000.f)) {  // maxSqDist
			enemyPositions.push_back(enemy->GetPos());
		}
	}

	path.clear();
	if (bestTarget != nullptr) {
		enemyPositions.clear();
		return bestTarget;
	}
	if (enemyPositions.empty()) {
		return nullptr;
	}

	AIFloat3 startPos = pos;
	circuit->GetPathfinder()->SetMapData(unit, threatMap, circuit->GetLastFrame());
	circuit->GetPathfinder()->FindBestPath(path, startPos, range * 0.5f, enemyPositions);
	enemyPositions.clear();

	return nullptr;
}

} // namespace circuit
