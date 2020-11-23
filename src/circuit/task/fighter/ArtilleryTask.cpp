/*
 * ArtilleryTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/ArtilleryTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/path/PathFinder.h"
#include "terrain/path/QueryPathSingle.h"
#include "terrain/path/QueryPathMulti.h"
#include "unit/action/MoveAction.h"
#include "unit/enemy/EnemyUnit.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CArtilleryTask::CArtilleryTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::ARTY, 1.f)
{
	position = manager->GetCircuit()->GetSetupManager()->GetBasePos();
}

CArtilleryTask::~CArtilleryTask()
{
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
	unit->PushTravelAct(travelAction);
	travelAction->StateWait();
}

void CArtilleryTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}

	unit->GetUnit()->SetMoveState(1);
}

void CArtilleryTask::Start(CCircuitUnit* unit)
{
	Execute(unit, false);
}

void CArtilleryTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();

	if (++updCount % 4 == 0) {
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

void CArtilleryTask::Execute(CCircuitUnit* unit, bool isUpdating)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	CEnemyInfo* bestTarget = FindTarget(unit, pos);

	if (bestTarget != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Attack(bestTarget->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			unit->CmdSetTarget(bestTarget);
		)
		unit->GetTravelAct()->StateWait();
		return;
	}

	if (!IsQueryReady(unit)) {
		return;
	}

	if (enemyPositions.empty()) {
		FallbackScout(unit, isUpdating);
		return;
	}

	CCircuitDef* cdef = unit->GetCircuitDef();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const float range = std::max(cdef->GetMaxRange()/* - threatMap->GetSquareSize()*/, (float)threatMap->GetSquareSize());

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathMultiQuery(
			unit, threatMap, frame,
			pos, range, enemyPositions);
	pathQueries[unit] = query;
	query->HoldTask(this);

	pathfinder->RunQuery(query, [this, isUpdating](const IPathQuery* query) {
		if (this->IsQueryAlive(query)) {
			this->ApplyTargetPath(static_cast<const CQueryPathMulti*>(query), isUpdating);
		}
	});
}

void CArtilleryTask::OnUnitIdle(CCircuitUnit* unit)
{
	IFighterTask::OnUnitIdle(unit);
	if (units.find(unit) != units.end()) {
		RemoveAssignee(unit);
	}
}

CEnemyInfo* CArtilleryTask::FindTarget(CCircuitUnit* unit, const AIFloat3& pos)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CThreatMap* threatMap = circuit->GetThreatMap();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const bool notAW = !cdef->HasSurfToWater();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	float range = cdef->GetMaxRange();
	float minSqDist = SQUARE(range);

	enemyPositions.clear();
	threatMap->SetThreatType(unit);
	bool isPosSafe = (threatMap->GetThreatAt(pos) <= THREAT_MIN);

	const CCircuitAI::EnemyInfos& enemies = circuit->GetEnemyInfos();
	if (isPosSafe) {
		// Трубка 15, прицел 120, бац, бац …и мимо!
		float maxThreat = .0f;
		CEnemyInfo* bestTarget = nullptr;
		CEnemyInfo* mediumTarget = nullptr;
		CEnemyInfo* worstTarget = nullptr;
		for (auto& kv : enemies) {
			CEnemyInfo* enemy = kv.second;
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
				if (edef->IsEnemyRoleAny(CCircuitDef::RoleMask::BUILDER)) {
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
			CEnemyInfo* enemy = kv.second;
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
			if (((targetCat & canTargetCat) == 0)
				|| ((targetCat & noChaseCat) != 0))
			{
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

	if (enemyPositions.empty()) {
		return nullptr;
	}

	return nullptr;
	// Return: <target>, startPos=pos, enemyPositions
}

void CArtilleryTask::ApplyTargetPath(const CQueryPathMulti* query, bool isUpdating)
{
	const std::shared_ptr<PathInfo>& pPath = query->GetPathInfo();
	CCircuitUnit* unit = query->GetUnit();

	if (!pPath->posPath.empty()) {
		if (manager->GetCircuit()->GetThreatMap()->GetThreatAt(pPath->posPath.back()) > THREAT_MIN) {
			FallbackBasePos(unit, isUpdating);
		} else {
			position = pPath->posPath.back();
			unit->GetTravelAct()->SetPath(pPath);
			unit->GetTravelAct()->StateActivate();
		}
	} else {
		FallbackBasePos(unit, isUpdating);
	}
}

void CArtilleryTask::FallbackBasePos(CCircuitUnit* unit, bool isUpdating)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CSetupManager* setupMgr = circuit->GetSetupManager();

	const AIFloat3& startPos = unit->GetPos(frame);
	position = setupMgr->GetBasePos();
	const float pathRange = DEFAULT_SLACK * 4;  // pathfinder->GetSquareSize()

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathSingleQuery(
			unit, circuit->GetThreatMap(), frame,
			startPos, position, pathRange);
	pathQueries[unit] = query;
	query->HoldTask(this);

	pathfinder->RunQuery(query, [this, isUpdating](const IPathQuery* query) {
		if (this->IsQueryAlive(query)) {
			this->ApplyBasePos(static_cast<const CQueryPathSingle*>(query), isUpdating);
		}
	});
}

void CArtilleryTask::ApplyBasePos(const CQueryPathSingle* query, bool isUpdating)
{
	const std::shared_ptr<PathInfo>& pPath = query->GetPathInfo();
	CCircuitUnit* unit = query->GetUnit();

	if (!pPath->posPath.empty()) {
		unit->GetTravelAct()->SetPath(pPath);
		unit->GetTravelAct()->StateActivate();
	} else {
		FallbackScout(unit, isUpdating);
	}
}

void CArtilleryTask::FallbackScout(CCircuitUnit* unit, bool isUpdating)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	const AIFloat3& threatPos = unit->GetTravelAct()->IsActive() ? position : pos;
	const bool proceed = isUpdating && (threatMap->GetThreatAt(unit, threatPos) < THREAT_MIN);
	if (!proceed) {
		position = circuit->GetMilitaryManager()->GetScoutPosition(unit);
	}

	if (!utils::is_valid(position) || !circuit->GetTerrainManager()->CanMoveToPos(unit->GetArea(), position)) {
		Fallback(unit, proceed);
		return;
	}

	const float pathRange = DEFAULT_SLACK * 4;  // pathfinder->GetSquareSize()

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathSingleQuery(
			unit, circuit->GetThreatMap(), frame,
			pos, position, pathRange);
	pathQueries[unit] = query;
	query->HoldTask(this);

	pathfinder->RunQuery(query, [this](const IPathQuery* query) {
		if (this->IsQueryAlive(query)) {
			this->ApplyScoutPath(static_cast<const CQueryPathSingle*>(query));
		}
	});
}

void CArtilleryTask::ApplyScoutPath(const CQueryPathSingle* query)
{
	const std::shared_ptr<PathInfo>& pPath = query->GetPathInfo();
	CCircuitUnit* unit = query->GetUnit();

	const bool proceed = pPath->path.size() > 2;
	if (proceed) {
		unit->GetTravelAct()->SetPath(pPath);
		unit->GetTravelAct()->StateActivate();
	} else {
		Fallback(unit, proceed);
	}
}

void CArtilleryTask::Fallback(CCircuitUnit* unit, bool proceed)
{
	if (proceed) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();

	float x = rand() % terrainMgr->GetTerrainWidth();
	float z = rand() % terrainMgr->GetTerrainHeight();
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	position = terrainMgr->GetMovePosition(unit->GetArea(), position);
	TRY_UNIT(circuit, unit,
		unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
	)
	unit->GetTravelAct()->StateWait();
}

} // namespace circuit
