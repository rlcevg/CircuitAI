/*
 * AntiAirTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/AntiAirTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/PathFinder.h"
#include "unit/action/MoveAction.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CAntiAirTask::CAntiAirTask(ITaskManager* mgr, float powerMod)
		: ISquadTask(mgr, FightType::AA, powerMod)
{
	CCircuitAI* circuit = manager->GetCircuit();
	float x = rand() % circuit->GetTerrainManager()->GetTerrainWidth();
	float z = rand() % circuit->GetTerrainManager()->GetTerrainHeight();
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
}

CAntiAirTask::~CAntiAirTask()
{
}

bool CAntiAirTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (!unit->GetCircuitDef()->IsRoleAA() ||
		(unit->GetCircuitDef() != leader->GetCircuitDef()))
	{
		return false;
	}
	const int frame = manager->GetCircuit()->GetLastFrame();
	if (leader->GetPos(frame).SqDistance2D(unit->GetPos(frame)) > SQUARE(1000.f)) {
		return false;
	}
	return true;
}

void CAntiAirTask::AssignTo(CCircuitUnit* unit)
{
	ISquadTask::AssignTo(unit);

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	CMoveAction* travelAction = new CMoveAction(unit, squareSize);
	unit->PushTravelAct(travelAction);
	travelAction->SetActive(false);
}

void CAntiAirTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);
	if (leader == nullptr) {
		manager->AbortTask(this);
	}
}

void CAntiAirTask::Start(CCircuitUnit* unit)
{
	if ((State::REGROUP == state) || (State::ENGAGE == state)) {
		return;
	}
	if (!pPath->posPath.empty()) {
		unit->GetTravelAct()->SetPath(pPath);
		unit->GetTravelAct()->SetActive(true);
	}
}

void CAntiAirTask::Update()
{
	++updCount;

	/*
	 * Check safety
	 */
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();

	if (State::DISENGAGE == state) {
		if (updCount % 32 == 1) {
			const float maxDist = std::max<float>(lowestRange, circuit->GetPathfinder()->GetSquareSize());
			if (position.SqDistance2D(leader->GetPos(frame)) < SQUARE(maxDist)) {
				state = State::ROAM;
			} else {
				AIFloat3 startPos = leader->GetPos(frame);
				CPathFinder* pathfinder = circuit->GetPathfinder();
				pathfinder->SetMapData(leader, circuit->GetThreatMap(), circuit->GetLastFrame());
				pathfinder->PreferPath(pPath->path);
				pathfinder->MakePath(*pPath, startPos, position, pathfinder->GetSquareSize());
				pathfinder->UnpreferPath();
				if (!pPath->posPath.empty()) {
					ActivePath();
					return;
				}
			}
		} else {
			return;
		}
	}

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

				unit->GetTravelAct()->SetActive(false);
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
			if (wasRegroup && !pPath->posPath.empty()) {
				ActivePath();
			}
			return;
		}
	}

	/*
	 * Update target
	 */
	FindTarget();

	state = State::ROAM;
	if (target != nullptr) {
		const float sqRange = SQUARE(lowestRange);
		for (CCircuitUnit* unit : units) {
			if (position.SqDistance2D(unit->GetPos(frame)) < sqRange) {
				state = State::ENGAGE;
				break;
			}
		}
		if (State::ENGAGE == state) {
			for (CCircuitUnit* unit : units) {
				unit->Attack(target->GetPos(), target, frame + FRAMES_PER_SEC * 60);

				unit->GetTravelAct()->SetActive(false);
			}
			return;
		}
	} else {
		static F3Vec ourPositions;  // NOTE: micro-opt
		AIFloat3 startPos = leader->GetPos(frame);
		circuit->GetMilitaryManager()->FillSafePos(startPos, leader->GetArea(), ourPositions);

		CPathFinder* pathfinder = circuit->GetPathfinder();
		pathfinder->SetMapData(leader, circuit->GetThreatMap(), circuit->GetLastFrame());
		pathfinder->PreferPath(pPath->path);
		pathfinder->FindBestPath(*pPath, startPos, DEFAULT_SLACK * 4, ourPositions, false);
		pathfinder->UnpreferPath();
		ourPositions.clear();

		if (!pPath->posPath.empty()) {
			position = pPath->posPath.back();
			ActivePath();
			return;
		} else {
			CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
			if ((commander != nullptr) &&
				circuit->GetTerrainManager()->CanMoveToPos(leader->GetArea(), commander->GetPos(frame)))
			{
				for (CCircuitUnit* unit : units) {
					unit->Guard(commander, frame + FRAMES_PER_SEC * 60);

					unit->GetTravelAct()->SetActive(false);
				}
				return;
			}
		}
	}
	if (pPath->posPath.empty()) {  // should never happen
		for (CCircuitUnit* unit : units) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			)

			unit->GetTravelAct()->SetActive(false);
		}
	} else {
		ActivePath();
	}
}

void CAntiAirTask::OnUnitIdle(CCircuitUnit* unit)
{
	ISquadTask::OnUnitIdle(unit);
	if ((leader == nullptr) || (State::DISENGAGE == state)) {
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
		Start(unit);  // NOTE: Not sure if it has effect
	}
}

void CAntiAirTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	ISquadTask::OnUnitDamaged(unit, attacker);

	if ((leader == nullptr) || (State::DISENGAGE == state) ||
		((attacker != nullptr) && (attacker->GetCircuitDef() != nullptr) && attacker->GetCircuitDef()->IsAbleToFly()))
	{
		return;
	}
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	static F3Vec ourPositions;  // NOTE: micro-opt
	AIFloat3 startPos = leader->GetPos(frame);
	circuit->GetMilitaryManager()->FillSafePos(startPos, leader->GetArea(), ourPositions);

	CPathFinder* pathfinder = circuit->GetPathfinder();
	pathfinder->SetMapData(leader, circuit->GetThreatMap(), circuit->GetLastFrame());
	pathfinder->PreferPath(pPath->path);
	pathfinder->FindBestPath(*pPath, startPos, DEFAULT_SLACK * 4, ourPositions);
	pathfinder->UnpreferPath();
	ourPositions.clear();

	if (!pPath->posPath.empty()) {
		position = pPath->posPath.back();
		ActivePath();
		state = State::DISENGAGE;
	} else {
		position = circuit->GetSetupManager()->GetBasePos();
		for (CCircuitUnit* unit : units) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			)

			unit->GetTravelAct()->SetActive(false);
		}
	}
}

void CAntiAirTask::FindTarget()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = leader->GetPos(circuit->GetLastFrame());
	STerrainMapArea* area = leader->GetArea();
	CCircuitDef* cdef = leader->GetCircuitDef();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float maxPower = attackPower * powerMod;

	CEnemyUnit* bestTarget = nullptr;
	float minSqDist = std::numeric_limits<float>::max();

	threatMap->SetThreatType(leader);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() ||
			(maxPower <= threatMap->GetThreatAt(enemy->GetPos()) - enemy->GetThreat()) ||
			!terrainManager->CanMoveToPos(area, enemy->GetPos()))
		{
			continue;
		}

		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			if (((edef->GetCategory() & canTargetCat) == 0) || ((edef->GetCategory() & noChaseCat) != 0)) {
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

	CPathFinder* pathfinder = circuit->GetPathfinder();
	pathfinder->SetMapData(leader, threatMap, circuit->GetLastFrame());
	pathfinder->PreferPath(pPath->path);
	pathfinder->MakePath(*pPath, startPos, endPos, pathfinder->GetSquareSize());
	pathfinder->UnpreferPath();
}

} // namespace circuit
