/*
 * AntiAirTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/AntiAirTask.h"
#include "task/TaskManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "terrain/PathFinder.h"
#include "unit/action/FightAction.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CAntiAirTask::CAntiAirTask(ITaskManager* mgr)
		: ISquadTask(mgr, FightType::AA)
{
	CCircuitAI* circuit = manager->GetCircuit();
	float x = rand() % (circuit->GetTerrainManager()->GetTerrainWidth() + 1);
	float z = rand() % (circuit->GetTerrainManager()->GetTerrainHeight() + 1);
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
}

CAntiAirTask::~CAntiAirTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CAntiAirTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (!unit->GetCircuitDef()->IsRoleAA() ||
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

void CAntiAirTask::AssignTo(CCircuitUnit* unit)
{
	ISquadTask::AssignTo(unit);

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	CFightAction* fightAction = new CFightAction(unit, squareSize);
	unit->PushBack(fightAction);
	fightAction->SetActive(false);
}

void CAntiAirTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);

	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CAntiAirTask::Execute(CCircuitUnit* unit)
{
	if (isRegroup || isAttack) {
		return;
	}
	if (pPath->empty()) {
		Update();
	} else {
		CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
		fightAction->SetPath(pPath);
		fightAction->SetActive(true);
	}
}

void CAntiAirTask::Update()
{
	++updCount;

	/*
	 * Merge tasks if possible
	 */
	if (updCount % 32 == 1) {
		ISquadTask* task = GetMergeTask();
		if (task != nullptr) {
			task->Merge(units, attackPower);
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

				CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
				fightAction->SetActive(false);
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
					CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
					fightAction->SetPath(pPath);
					fightAction->SetActive(true);
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
				const AIFloat3& pos = utils::get_radial_pos(target->GetPos(), SQUARE_SIZE * 8);
				TRY_UNIT(circuit, unit,
					unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
					unit->GetUnit()->ExecuteCustomCommand(CMD_UNIT_SET_TARGET, {(float)target->GetId()});
				)

				CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
				fightAction->SetActive(false);
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

				CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
				fightAction->SetActive(false);
			}
			return;
		}
	}
	if (pPath->empty()) {  // should never happen
		for (CCircuitUnit* unit : units) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			)

			CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
			fightAction->SetActive(false);
		}
	} else {
		for (CCircuitUnit* unit : units) {
			CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
			fightAction->SetPath(pPath);
			fightAction->SetActive(true);
		}
	}
}

void CAntiAirTask::OnUnitIdle(CCircuitUnit* unit)
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

	CEnemyUnit* bestTarget = nullptr;
	float minSqDist = std::numeric_limits<float>::max();

	threatMap->SetThreatType(leader);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden()) {
			continue;
		}
		float threat = threatMap->GetThreatAt(enemy->GetPos());
		if ((attackPower <= threat - enemy->GetThreat()) || !terrainManager->CanMoveToPos(area, enemy->GetPos())) {
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
	pPath->clear();

	CPathFinder* pathfinder = circuit->GetPathfinder();
	pathfinder->SetMapData(leader, threatMap, circuit->GetLastFrame());
	pathfinder->MakePath(*pPath, startPos, endPos, pathfinder->GetSquareSize());
}

} // namespace circuit
