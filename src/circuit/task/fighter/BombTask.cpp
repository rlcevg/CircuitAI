/*
 * BombTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/BombTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/MilitaryManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/PathFinder.h"
#include "unit/action/MoveAction.h"
#include "unit/enemy/EnemyUnit.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CBombTask::CBombTask(ITaskManager* mgr, float powerMod)
		: IFighterTask(mgr, FightType::BOMB, powerMod)
{
}

CBombTask::~CBombTask()
{
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
	unit->PushTravelAct(travelAction);
	travelAction->SetActive(false);
}

void CBombTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CBombTask::Start(CCircuitUnit* unit)
{
	Execute(unit, false);
}

void CBombTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();

	if (++updCount % 4 == 0) {
		for (CCircuitUnit* unit : units) {
			Execute(unit, true);
		}
	} else {
		for (CCircuitUnit* unit : units) {
			if (unit->IsForceExecute(frame)) {
				Execute(unit, true);
			}
		}
	}
}

void CBombTask::Execute(CCircuitUnit* unit, bool isUpdating)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	if (!unit->IsWeaponReady(frame)) {  // reload empty unit
		if (updCount % 32 == 0) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->ExecuteCustomCommand(CMD_FIND_PAD, {}, 0, frame + FRAMES_PER_SEC * 60);
			)
		}
		SetTarget(nullptr);
		return;
	}

	const AIFloat3& pos = unit->GetPos(frame);
	std::shared_ptr<PathInfo> pPath = std::make_shared<PathInfo>();
	CEnemyInfo* lastTarget = target;
	SetTarget(nullptr);
	SetTarget(FindTarget(unit, lastTarget, pos, *pPath));

	if (target != nullptr) {
		position = target->GetPos();
		TRY_UNIT(circuit, unit,
			if (target->GetUnit()->IsCloaked()) {
				unit->GetUnit()->ExecuteCustomCommand(CMD_ATTACK_GROUND, {position.x, position.y, position.z},
													  UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			} else if (lastTarget != target) {
				unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			}
		)
		unit->GetTravelAct()->SetActive(false);
		return;
	} else if (!pPath->posPath.empty()) {
		position = pPath->posPath.back();
		unit->GetTravelAct()->SetPath(pPath);
		unit->GetTravelAct()->SetActive(true);
		return;
	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& threatPos = unit->GetTravelAct()->IsActive() ? position : pos;
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

		proceed = pPath->path.size() > 2;
		if (proceed) {
//			position = path.back();
			unit->GetTravelAct()->SetPath(pPath);
			unit->GetTravelAct()->SetActive(true);
			return;
		}
	}

	if (proceed) {
		return;
	}
	float x = rand() % terrainManager->GetTerrainWidth();
	float z = rand() % terrainManager->GetTerrainHeight();
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	TRY_UNIT(circuit, unit,
		unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
	)
	unit->GetTravelAct()->SetActive(false);
}

void CBombTask::OnUnitIdle(CCircuitUnit* unit)
{
	IFighterTask::OnUnitIdle(unit);
	if (units.find(unit) != units.end()) {
		Update();
	}
}

void CBombTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	// Do not retreat if bomber is close to target
	if (target == nullptr) {
		IFighterTask::OnUnitDamaged(unit, attacker);
	} else {
		const AIFloat3& pos = unit->GetPos(manager->GetCircuit()->GetLastFrame());
		if (pos.SqDistance2D(target->GetPos()) > SQUARE(unit->GetCircuitDef()->GetLosRadius())) {
			IFighterTask::OnUnitDamaged(unit, attacker);
		}
	}
}

CEnemyInfo* CBombTask::FindTarget(CCircuitUnit* unit, CEnemyInfo* lastTarget, const AIFloat3& pos, PathInfo& path)
{
	// TODO: 1) Bombers should constantly harass undefended targets and not suicide.
	//       2) Fat target getting close to base should gain priority and be attacked by group if high AA threat.
	//       3) Avoid RoleAA targets.
	CCircuitAI* circuit = manager->GetCircuit();
	CThreatMap* threatMap = circuit->GetThreatMap();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const bool notAW = !cdef->HasAntiWater();
	const float scale = (cdef->GetMinRange() > 300.0f) ? 4.0f : 1.0f;
	const float maxPower = threatMap->GetUnitThreat(unit) * scale * powerMod;
//	const float maxAltitude = cdef->GetAltitude();
	const float speed = cdef->GetSpeed() / 1.75f;
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
//	const float range = std::max(unit->GetUnit()->GetMaxRange() + threatMap->GetSquareSize(),
//								 cdef->GetLosRadius()) * 2;
	const float sqRange = (lastTarget != nullptr) ? pos.SqDistance2D(lastTarget->GetPos()) + 1.f : SQUARE(2000.0f);
	float maxThreat = .0f;

	COOAICallback* callback = circuit->GetCallback();
	float aoe = std::min(cdef->GetAoe() + SQUARE_SIZE, DEFAULT_SLACK * 2.f);
	std::function<bool (const AIFloat3& pos)> noAllies = [](const AIFloat3& pos) {
		return true;
	};
	if (aoe > SQUARE_SIZE * 2) {
		noAllies = [callback, aoe](const AIFloat3& pos) {
			return !callback->IsFriendlyUnitsIn(pos, aoe);
		};
	}

	CEnemyInfo* bestTarget = nullptr;
	CEnemyInfo* mediumTarget = nullptr;
	CEnemyInfo* worstTarget = nullptr;
	static F3Vec enemyPositions;  // NOTE: micro-opt
	threatMap->SetThreatType(unit);
	const CCircuitAI::EnemyInfos& enemies = circuit->GetEnemyInfos();
	for (auto& kv : enemies) {
		CEnemyInfo* enemy = kv.second;
		if (enemy->IsHidden()) {
			continue;
		}
		float power = threatMap->GetThreatAt(enemy->GetPos()) - enemy->GetThreat();
		if ((maxPower <= power) ||
			(notAW && (enemy->GetPos().y < -SQUARE_SIZE * 5)))
		{
			continue;
		}

		int targetCat;
//		float altitude;
		float defThreat;
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
			defThreat = edef->GetPower();
			isBuilder = edef->IsEnemyRoleAny(CCircuitDef::RoleMask::BUILDER | CCircuitDef::RoleMask::COMM);
		} else {
			targetCat = UNKNOWN_CATEGORY;
//			altitude = 0.f;
			defThreat = enemy->GetThreat();
			isBuilder = false;
		}

		float sumPower = 0.f;
		for (IFighterTask* task : enemy->GetTasks()) {
			sumPower += task->GetAttackPower();
		}
		if (sumPower > defThreat) {
			continue;
		}

		float sqDist = pos.SqDistance2D(enemy->GetPos());
		if ((sqDist < sqRange) && enemy->IsInRadarOrLOS()/* && (altitude < maxAltitude)*/) {
			if (isBuilder) {
				if (noAllies(enemy->GetPos())) {
					bestTarget = enemy;
					maxThreat = std::numeric_limits<float>::max();
				}
			} else if (maxThreat <= defThreat) {
				if (noAllies(enemy->GetPos())) {
					bestTarget = enemy;
					maxThreat = defThreat;
				}
			} else if ((bestTarget == nullptr) && noAllies(enemy->GetPos())) {
				if ((targetCat & noChaseCat) == 0) {
					mediumTarget = enemy;
				} else if (mediumTarget == nullptr) {
					worstTarget = enemy;
				}
			}
			continue;
		}
//		if (sqDist < SQUARE(2000.f)) {  // maxSqDist
			enemyPositions.push_back(enemy->GetPos());
//		}
	}
	if (bestTarget == nullptr) {
		bestTarget = (mediumTarget != nullptr) ? mediumTarget : worstTarget;
	}

	path.Clear();
	if (bestTarget != nullptr) {
		enemyPositions.clear();
		return bestTarget;
	}
	if (enemyPositions.empty()) {
		return nullptr;
	}

	AIFloat3 startPos = pos;
	const float range = std::max<float>(cdef->GetLosRadius(), threatMap->GetSquareSize());
	circuit->GetPathfinder()->SetMapData(unit, threatMap, circuit->GetLastFrame());
	circuit->GetPathfinder()->FindBestPath(path, startPos, range, enemyPositions);
	enemyPositions.clear();

	return nullptr;
}

} // namespace circuit
