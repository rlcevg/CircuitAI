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
#include "unit/action/FightAction.h"
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
	ITravelAction* travelAction;
	if (unit->GetCircuitDef()->IsAttrSiege()) {
		travelAction = new CFightAction(unit, squareSize);
	} else {
		travelAction = new CMoveAction(unit, squareSize);
	}
	unit->PushBack(travelAction);
	travelAction->SetActive(false);
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
	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();

	if ((++updCount % 2 == 0) && (frame >= lastTouched + FRAMES_PER_SEC)) {
		lastTouched = frame;
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
	if (!act->IsAny(IUnitAction::Mask::MOVE | IUnitAction::Mask::FIGHT | IUnitAction::Mask::JUMP)) {
		return;
	}
	ITravelAction* travelAction = static_cast<ITravelAction*>(act);

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	std::shared_ptr<F3Vec> pPath = std::make_shared<F3Vec>();
	SetTarget(nullptr);  // make adequate enemy->GetTasks().size()
	SetTarget(FindTarget(unit, pos, *pPath));

	if (target != nullptr) {
		position = target->GetPos();
		if (target->GetUnit()->IsCloaked()) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->ExecuteCustomCommand(CMD_ATTACK_GROUND, {position.x, position.y, position.z},
													  UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			)
		} else {
			unit->Attack(target, frame + FRAMES_PER_SEC * 60);
		}
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
	// NOTE: Use max(unit_threat, THREAT_MIN) for no-weapon scouts
	bool proceed = isUpdating && (threatMap->GetThreatAt(unit, threatPos) < std::max(threatMap->GetUnitThreat(unit), THREAT_MIN) * 0.75f);
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
	Map* map = circuit->GetMap();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	STerrainMapArea* area = unit->GetArea();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const bool notAW = !cdef->HasAntiWater();
	const bool notAA = !cdef->HasAntiAir();
	const float speed = SQUARE(cdef->GetSpeed() * 1.1f);
	const float maxPower = threatMap->GetUnitThreat(unit) * 0.75f;
	const float weaponRange = cdef->GetMaxRange();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float range = std::max(unit->GetUnit()->GetMaxRange() + threatMap->GetSquareSize() * 2,
								 /*unit->IsUnderWater(frame) ? cdef->GetSonarRadius() : */cdef->GetLosRadius());
	float minSqDist = SQUARE(range);
	float maxThreat = 0.f;
	float minPower = maxPower;

	CEnemyUnit* bestTarget = nullptr;
	CEnemyUnit* worstTarget = nullptr;
	static F3Vec enemyPositions;  // NOTE: micro-opt
	threatMap->SetThreatType(unit);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden()/* || !enemy->GetTasks().empty()*/) {
			continue;
		}
		const AIFloat3& ePos = enemy->GetPos();
		const float power = threatMap->GetThreatAt(ePos);
		if ((maxPower <= power) ||
			!terrainManager->CanMoveToPos(area, ePos) ||
			(notAW && (ePos.y < -SQUARE_SIZE * 5)) ||
			(enemy->GetUnit()->GetVel().SqLength2D() >= speed))
		{
			continue;
		}

		int targetCat;
		float defPower;
		bool isBuilder;
		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			targetCat = edef->GetCategory();
			if (((targetCat & canTargetCat) == 0) ||
				(edef->IsAbleToFly() && notAA) ||
				(ePos.y - map->GetElevationAt(ePos.x, ePos.z) > weaponRange))
			{
				continue;
			}
			defPower = edef->GetPower();
			isBuilder = edef->IsRoleBuilder();
		} else {
			targetCat = UNKNOWN_CATEGORY;
			defPower = enemy->GetThreat();
			isBuilder = false;
		}

		float sqDist = pos.SqDistance2D(ePos);
		if ((minPower > power) && (minSqDist > sqDist)) {
			if (enemy->IsInRadarOrLOS()) {
//				AIFloat3 dir = enemy->GetUnit()->GetPos() - pos;
//				float rayRange = dir.LengthNormalize();
//				CCircuitUnit::Id hitUID = circuit->GetDrawer()->TraceRay(pos, dir, rayRange, u, 0);
//				if (hitUID == enemy->GetId()) {
					if (((targetCat & noChaseCat) == 0) && !enemy->GetUnit()->IsBeingBuilt()) {
						if (isBuilder) {
							bestTarget = enemy;
							minSqDist = sqDist;
							maxThreat = std::numeric_limits<float>::max();
						} else if (maxThreat <= defPower) {
							bestTarget = enemy;
							minSqDist = sqDist;
							maxThreat = defPower;
						}
						minPower = power;
					} else if (bestTarget == nullptr) {
						worstTarget = enemy;
					}
//				}
			}
			continue;
		}
//		if (sqDist < SQUARE(2000.f)) {  // maxSqDist
			enemyPositions.push_back(ePos);
//		}
	}
	if (bestTarget == nullptr) {
		bestTarget = worstTarget;
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
