/*
 * ScoutTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/ScoutTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/MilitaryManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/path/PathFinder.h"
#include "terrain/path/QueryPathSingle.h"
#include "terrain/path/QueryPathMulti.h"
#include "unit/action/MoveAction.h"
#include "unit/action/FightAction.h"
#include "unit/enemy/EnemyUnit.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringMap.h"

#include "AISCommands.h"
//#include "Drawer.h"  // TraceRay

namespace circuit {

using namespace springai;

CScoutTask::CScoutTask(ITaskManager* mgr, float powerMod)
		: IFighterTask(mgr, FightType::SCOUT, powerMod)
{
}

CScoutTask::~CScoutTask()
{
}

bool CScoutTask::CanAssignTo(CCircuitUnit* unit) const
{
	return units.empty() && unit->GetCircuitDef()->IsRoleScout();
}

void CScoutTask::AssignTo(CCircuitUnit* unit)
{
	IFighterTask::AssignTo(unit);

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	CCircuitDef* cdef = unit->GetCircuitDef();
	ITravelAction* travelAction;
	if (cdef->IsAttrSiege()) {
		travelAction = new CFightAction(unit, squareSize);
	} else {
		travelAction = new CMoveAction(unit, squareSize);
	}
	unit->PushTravelAct(travelAction);
	travelAction->StateWait();
	unit->SetAllowedToJump(cdef->IsAbleToJump() && cdef->IsAttrJump());
}

void CScoutTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CScoutTask::Start(CCircuitUnit* unit)
{
	Execute(unit, false);
}

void CScoutTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();

	if ((++updCount % 2 == 0) && (frame >= lastTouched + FRAMES_PER_SEC)) {
		lastTouched = frame;
		for (CCircuitUnit* unit : units) {
			Execute(unit, true);
		}
	} else {
		for (CCircuitUnit* unit : units) {
			if (unit->IsForceUpdate(frame)) {
				Execute(unit, true);
			}
		}
	}
}

void CScoutTask::OnUnitIdle(CCircuitUnit* unit)
{
	IFighterTask::OnUnitIdle(unit);
	if (units.find(unit) != units.end()) {
		RemoveAssignee(unit);
	}
}

void CScoutTask::Execute(CCircuitUnit* unit, bool isUpdating)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);

	const bool isTargetsFound = FindTarget(unit, pos);

	if (target != nullptr) {
		position = target->GetPos();
		if (target->GetUnit()->IsCloaked()) {
			TRY_UNIT(circuit, unit,
				unit->CmdAttackGround(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			)
		} else {
			unit->Attack(target, frame + FRAMES_PER_SEC * 60);
		}
		unit->GetTravelAct()->StateWait();
		return;
	}

	if (!IsQueryReady(unit)) {
		return;
	}

	if (!isTargetsFound) {  // enemyPositions.empty()
		FallbackScout(unit, isUpdating);
		return;
	}

	CCircuitDef* cdef = unit->GetCircuitDef();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const float range = std::max(unit->GetUnit()->GetMaxRange() + threatMap->GetSquareSize() * 2,
								 /*unit->IsUnderWater(frame) ? cdef->GetSonarRadius() : */cdef->GetLosRadius());

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathMultiQuery(
			unit, threatMap, frame,
			pos, range * 0.5f, enemyPositions, nullptr, attackPower);
	pathQueries[unit] = query;
	query->HoldTask(this);

	pathfinder->RunQuery(query, [this, isUpdating](const IPathQuery* query) {
		if (this->IsQueryAlive(query)) {
			this->ApplyTargetPath(static_cast<const CQueryPathMulti*>(query), isUpdating);
		}
	});
}

bool CScoutTask::FindTarget(CCircuitUnit* unit, const AIFloat3& pos)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CMap* map = circuit->GetMap();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	STerrainMapArea* area = unit->GetArea();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const bool notAW = !cdef->HasAntiWater();
	const bool notAA = !cdef->HasAntiAir();
	const float speed = SQUARE(cdef->GetSpeed() * 0.8f / FRAMES_PER_SEC);
	const float maxPower = threatMap->GetUnitThreat(unit) * powerMod;
	const float weaponRange = cdef->GetMaxRange();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float range = std::max(unit->GetUnit()->GetMaxRange() + threatMap->GetSquareSize() * 2,
								 /*unit->IsUnderWater(frame) ? cdef->GetSonarRadius() : */cdef->GetLosRadius());
	float minSqDist = SQUARE(range);
	float maxThreat = 0.f;
	float minPower = maxPower;

	SetTarget(nullptr);  // make adequate enemy->GetTasks().size()
	CEnemyInfo* bestTarget = nullptr;
	CEnemyInfo* worstTarget = nullptr;
	enemyPositions.clear();
	threatMap->SetThreatType(unit);
	const CCircuitAI::EnemyInfos& enemies = circuit->GetEnemyInfos();
	for (auto& kv : enemies) {
		CEnemyInfo* enemy = kv.second;
		if (enemy->IsHidden() || (enemy->GetTasks().size() > 2)) {
			continue;
		}
		const AIFloat3& ePos = enemy->GetPos();
		const float power = threatMap->GetThreatAt(ePos);
		if ((maxPower <= power)
			|| !terrainMgr->CanMoveToPos(area, ePos)
			|| (enemy->GetVel().SqLength2D() >= speed))
		{
			continue;
		}

		int targetCat;
		float defThreat;
		bool isBuilder;
		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			targetCat = edef->GetCategory();
			if (((targetCat & canTargetCat) == 0)
				|| (edef->IsAbleToFly() && notAA))
			{
				continue;
			}
			float elevation = map->GetElevationAt(ePos.x, ePos.z);
			if ((notAW && !edef->IsYTargetable(elevation, ePos.y))
				|| (ePos.y - elevation > weaponRange))
			{
				continue;
			}
			defThreat = edef->GetPower();
			isBuilder = edef->IsEnemyRoleAny(CCircuitDef::RoleMask::BUILDER);
		} else {
			if (notAW && (ePos.y < -SQUARE_SIZE * 5)) {
				continue;
			}
			targetCat = UNKNOWN_CATEGORY;
			defThreat = enemy->GetThreat();
			isBuilder = false;
		}

		float sqDist = pos.SqDistance2D(ePos);
		if ((minPower > power) && (minSqDist > sqDist)) {
			if (enemy->IsInRadarOrLOS()) {
//				AIFloat3 dir = enemy->GetPos() - pos;
//				float rayRange = dir.LengthNormalize();
//				CUnit::Id hitUID = circuit->GetDrawer()->TraceRay(pos, dir, rayRange, u, 0);
//				if (hitUID == enemy->GetId()) {
					if (((targetCat & noChaseCat) == 0) && !enemy->IsBeingBuilt()) {
						if (isBuilder) {
							bestTarget = enemy;
							minSqDist = sqDist;
							maxThreat = std::numeric_limits<float>::max();
						} else if (maxThreat <= defThreat) {
							bestTarget = enemy;
							minSqDist = sqDist;
							maxThreat = defThreat;
						}
						minPower = power;
					} else if (bestTarget == nullptr) {
						worstTarget = enemy;
					}
//				}
			}
			continue;
		}
		if (sqDist < SQUARE(1000.f)) {  // maxSqDist
			enemyPositions.push_back(ePos);
		}
	}
	if (bestTarget == nullptr) {
		bestTarget = worstTarget;
	}

	if (bestTarget != nullptr) {
		SetTarget(bestTarget);
		return true;
	}
	if (enemyPositions.empty()) {
		return false;
	}

	return true;
	// Return: target=bestTarget, startPos=pos, enemyPositions
}

void CScoutTask::ApplyTargetPath(const CQueryPathMulti* query, bool isUpdating)
{
	const std::shared_ptr<PathInfo>& pPath = query->GetPathInfo();
	CCircuitUnit* unit = query->GetUnit();

	if (!pPath->posPath.empty()) {
		position = pPath->posPath.back();
		unit->GetTravelAct()->SetPath(pPath);
		unit->GetTravelAct()->StateActivate();
	} else {
		FallbackScout(unit, isUpdating);
	}
}

void CScoutTask::FallbackScout(CCircuitUnit* unit, bool isUpdating)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = unit->GetPos(frame);
	const AIFloat3& threatPos = unit->GetTravelAct()->IsActive() ? position : pos;
	// NOTE: Use max(unit_threat, THREAT_MIN) for no-weapon scouts
	bool proceed = isUpdating && (threatMap->GetThreatAt(unit, threatPos) < std::max(threatMap->GetUnitThreat(unit), THREAT_MIN) * powerMod);
	if (!proceed) {
		position = circuit->GetMilitaryManager()->GetScoutPosition(unit);
	}

	if (!utils::is_valid(position) && !proceed) {
		float x = rand() % terrainMgr->GetTerrainWidth();
		float z = rand() % terrainMgr->GetTerrainHeight();
		position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
		position = terrainMgr->GetMovePosition(unit->GetArea(), position);
	}

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathSingleQuery(
			unit, threatMap, frame,
			pos, position, pathfinder->GetSquareSize());
	pathQueries[unit] = query;
	query->HoldTask(this);

	pathfinder->RunQuery(query, [this](const IPathQuery* query) {
		if (this->IsQueryAlive(query)) {
			this->ApplyScoutPath(static_cast<const CQueryPathSingle*>(query));
		}
	});
}

void CScoutTask::ApplyScoutPath(const CQueryPathSingle* query)
{
	const std::shared_ptr<PathInfo>& pPath = query->GetPathInfo();
	CCircuitUnit* unit = query->GetUnit();

	if (pPath->path.size() > 2) {
//		position = path.back();
		unit->GetTravelAct()->SetPath(pPath);
		unit->GetTravelAct()->StateActivate();
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	TRY_UNIT(circuit, unit,
		unit->CmdMoveTo(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
	)
	unit->GetTravelAct()->StateWait();
}

} // namespace circuit
