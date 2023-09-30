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
#include "terrain/path/QueryPathMulti.h"
#include "unit/action/FightAction.h"
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
	Execute(unit);
}

void CArtilleryTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();

	if (++updCount % 4 == 0) {
		for (CCircuitUnit* unit : units) {
			Execute(unit);
		}
	} else {
		for (CCircuitUnit* unit : units) {
			if (unit->IsForceUpdate(frame)) {
				Execute(unit);
			}
		}
	}
}

void CArtilleryTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	CEnemyInfo* bestTarget = FindTarget(unit, pos);

	if (bestTarget != nullptr) {
		TRY_UNIT(circuit, unit,
			if (!circuit->IsCheating()) {
				unit->GetUnit()->Attack(bestTarget->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			} else {
				unit->CmdAttackGround(bestTarget->GetPos(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			}
			unit->CmdSetTarget(bestTarget);
		)
		unit->GetTravelAct()->StateWait();
		return;
	}

	if (!IsQueryReady(unit)) {
		return;
	}

	if (enemyPositions.empty()) {
		FallbackSafePos(unit);
		return;
	}

	CCircuitDef* cdef = unit->GetCircuitDef();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const float range = std::max(cdef->GetMaxRange()/* - threatMap->GetSquareSize()*/, (float)threatMap->GetSquareSize());

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathMultiQuery(
			unit, threatMap,
			pos, range, enemyPositions);
	pathQueries[unit] = query;

	pathfinder->RunQuery(circuit->GetScheduler().get(), query, [this](const IPathQuery* query) {
		this->ApplyTargetPath(static_cast<const CQueryPathMulti*>(query));
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
			if (enemy->IsHidden()
				|| !enemy->IsInRadarOrLOS()
				|| (notAW && (enemy->GetPos().y < -SQUARE_SIZE * 5)))
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
			if (enemy->IsHidden()
				|| !enemy->IsInRadarOrLOS()
				|| (notAW && (enemy->GetPos().y < -SQUARE_SIZE * 5)))
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

void CArtilleryTask::ApplyTargetPath(const CQueryPathMulti* query)
{
	const std::shared_ptr<CPathInfo>& pPath = query->GetPathInfo();
	CCircuitUnit* unit = query->GetUnit();

	if (!pPath->posPath.empty()) {
		if (manager->GetCircuit()->GetThreatMap()->GetThreatAt(pPath->posPath.back()) > THREAT_MIN) {
			FallbackSafePos(unit);
		} else {
			position = pPath->posPath.back();
			unit->GetTravelAct()->SetPath(pPath);
		}
	} else {
		FallbackSafePos(unit);
	}
}

void CArtilleryTask::FallbackSafePos(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	circuit->GetMilitaryManager()->FillSafePos(unit, urgentPositions);
	if (urgentPositions.empty()) {
		Fallback(unit, false);
		return;
	}

	const AIFloat3& startPos = unit->GetPos(circuit->GetLastFrame());
	const float pathRange = DEFAULT_SLACK * 4;

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathMultiQuery(
			unit, circuit->GetThreatMap(),
			startPos, pathRange, urgentPositions);
	pathQueries[unit] = query;

	pathfinder->RunQuery(circuit->GetScheduler().get(), query, [this](const IPathQuery* query) {
		this->ApplySafePos(static_cast<const CQueryPathMulti*>(query));
	});
}

void CArtilleryTask::ApplySafePos(const CQueryPathMulti* query)
{
	const std::shared_ptr<CPathInfo>& pPath = query->GetPathInfo();
	CCircuitUnit* unit = query->GetUnit();

	const bool proceed = pPath->path.size() > 2;
	if (proceed) {
		unit->GetTravelAct()->SetPath(pPath);
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
		unit->CmdFightTo(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
	)
	unit->GetTravelAct()->StateWait();
}

} // namespace circuit
