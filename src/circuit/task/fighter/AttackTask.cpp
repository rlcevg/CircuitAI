/*
 * AttackTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/AttackTask.h"
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

CAttackTask::CAttackTask(ITaskManager* mgr)
		: ISquadTask(mgr, FightType::ATTACK)
		, pPath(std::make_shared<F3Vec>())
		, minPower(.0f)
{
	CCircuitAI* circuit = manager->GetCircuit();
	float x = rand() % (circuit->GetTerrainManager()->GetTerrainWidth() + 1);
	float z = rand() % (circuit->GetTerrainManager()->GetTerrainHeight() + 1);
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
}

CAttackTask::~CAttackTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CAttackTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (leader == nullptr) {
		return true;
	}
	int frame = manager->GetCircuit()->GetLastFrame();
	if (leader->GetPos(frame).SqDistance2D(unit->GetPos(frame)) > SQUARE(1000.f)) {
		return false;
	}
	if ((leader->GetCircuitDef()->IsAbleToFly() && unit->GetCircuitDef()->IsAbleToFly()) ||
		(leader->GetCircuitDef()->IsAmphibious() && unit->GetCircuitDef()->IsAmphibious()) ||
		(leader->GetCircuitDef()->IsLander() && unit->GetCircuitDef()->IsLander()) ||
		(leader->GetCircuitDef()->IsFloater() && unit->GetCircuitDef()->IsFloater()))
	{
		return true;
	}
	return false;
}

void CAttackTask::AssignTo(CCircuitUnit* unit)
{
	ISquadTask::AssignTo(unit);

	minPower += unit->GetCircuitDef()->GetPower() / 8;

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	CFightAction* fightAction = new CFightAction(unit, squareSize);
	unit->PushBack(fightAction);
	fightAction->SetActive(false);
}

void CAttackTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);

	if (attackPower < minPower) {
		manager->AbortTask(this);
	}
}

void CAttackTask::Execute(CCircuitUnit* unit)
{
	if (isRegroup || isAttack) {
		return;
	}
	if (pPath->empty()) {
		Update();
	} else {
		CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
		fightAction->SetPath(pPath, lowestSpeed);
		fightAction->SetActive(true);
//		unit->Update(manager->GetCircuit());  // NOTE: Do not spam commands
	}
}

void CAttackTask::Update()
{
	++updCount;
	bool wasRegroup = isRegroup;
	bool mustRegroup = IsMustRegroup();
	if (isRegroup) {
		if (mustRegroup) {
			CCircuitAI* circuit = manager->GetCircuit();
			int frame = circuit->GetLastFrame();
			const AIFloat3& groupPos = leader->GetPos(frame);
			for (CCircuitUnit* unit : units) {
				const AIFloat3& pos = utils::get_near_pos(groupPos, SQUARE_SIZE * 32);
				unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
				unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);

				CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
				fightAction->SetActive(false);
			}
		}
		return;
	}

	bool isExecute = (updCount % 4 == 0);
	if (!isExecute) {
		for (CCircuitUnit* unit : units) {
			isExecute |= unit->IsForceExecute();
		}
	}
	if (!isExecute) {
		if (wasRegroup) {
			for (CCircuitUnit* unit : units) {
				CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
				fightAction->SetPath(pPath, lowestSpeed);
				fightAction->SetActive(true);
			}
		}
		ISquadTask::Update();
		return;
	}

	FindTarget();

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	isAttack = false;
	if (target != nullptr) {
		const float sqHighestRange = highestRange * highestRange;
		for (CCircuitUnit* unit : units) {
			if (position.SqDistance2D(unit->GetPos(frame)) < sqHighestRange) {
				isAttack = true;
				break;
			}
		}
		if (isAttack) {
			for (CCircuitUnit* unit : units) {
				const AIFloat3& pos = utils::get_radial_pos(target->GetPos(), SQUARE_SIZE * 8);
				unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
				unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);
				unit->GetUnit()->ExecuteCustomCommand(CMD_UNIT_SET_TARGET, {(float)target->GetId()});

				CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
				fightAction->SetActive(false);
			}
			return;
		}
	} else {
		CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
		if ((frame > FRAMES_PER_SEC * 300) && (commander != nullptr) &&
			circuit->GetTerrainManager()->CanMoveToPos(commander->GetArea(), commander->GetPos(frame)))
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
			unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			unit->GetUnit()->SetWantedMaxSpeed(lowestSpeed);

			CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
			fightAction->SetActive(false);
		}
	} else {
		for (CCircuitUnit* unit : units) {
			CFightAction* fightAction = static_cast<CFightAction*>(unit->End());
			fightAction->SetPath(pPath, lowestSpeed);
			fightAction->SetActive(true);
			unit->Update(circuit);
		}
	}
}

void CAttackTask::OnUnitIdle(CCircuitUnit* unit)
{
	ISquadTask::OnUnitIdle(unit);
	if (units.empty()) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	if (position.SqDistance2D(leader->GetPos(circuit->GetLastFrame())) < SQUARE(lowestRange)) {
		float x = rand() % (circuit->GetTerrainManager()->GetTerrainWidth() + 1);
		float z = rand() % (circuit->GetTerrainManager()->GetTerrainHeight() + 1);
		position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	}

	if (units.find(unit) != units.end()) {
		Execute(unit);  // NOTE: Not sure if it has effect
	}
}

void CAttackTask::FindTarget()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = leader->GetPos(circuit->GetLastFrame());
	STerrainMapArea* area = leader->GetArea();
	CCircuitDef* cdef = leader->GetCircuitDef();
	const float speed = cdef->GetSpeed();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();

	target = nullptr;
	float minSqDist = std::numeric_limits<float>::max();

	threatMap->SetThreatType(leader);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden()) {
			continue;
		}
		float threat = threatMap->GetThreatAt(enemy->GetPos());
		if ((attackPower <= threat) || !terrainManager->CanMoveToPos(area, enemy->GetPos())) {
			continue;
		}
		if (!cdef->HasAntiWater() && (enemy->GetPos().y < -SQUARE_SIZE * 5)) {
			continue;
		}
		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			if (edef->GetSpeed() > speed) {
				continue;
			}
			if (((edef->GetCategory() & canTargetCat) == 0) || ((edef->GetCategory() & noChaseCat) != 0)) {
				continue;
			}
			if (enemy->GetUnit()->IsBeingBuilt()) {
				continue;
			}
		}

		const float sqDist = pos.SqDistance2D(enemy->GetPos());
		if (sqDist < minSqDist) {
			target = enemy;
			minSqDist = sqDist;
		}
	}

	if (target != nullptr) {
		position = target->GetPos();
	}
	AIFloat3 startPos = pos;
	AIFloat3 endPos = position;
	pPath->clear();

	CPathFinder* pathfinder = circuit->GetPathfinder();
	pathfinder->SetMapData(leader, threatMap, circuit->GetLastFrame());
	pathfinder->MakePath(*pPath, startPos, endPos, std::max<float>(lowestRange, pathfinder->GetSquareSize()));
}

} // namespace circuit
