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
#include "unit/action/MoveAction.h"
#include "unit/action/SupportAction.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

#define MIN_POWER_DIV	8

CAttackTask::CAttackTask(ITaskManager* mgr)
		: ISquadTask(mgr, FightType::ATTACK)
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
	assert(leader != nullptr);
	int frame = manager->GetCircuit()->GetLastFrame();
	if (leader->GetPos(frame).SqDistance2D(unit->GetPos(frame)) > SQUARE(1000.f)) {
		return false;
	}
	if (unit->GetCircuitDef()->IsAmphibious() &&
		(leader->GetCircuitDef()->IsAmphibious() || leader->GetCircuitDef()->IsLander() || leader->GetCircuitDef()->IsFloater()))
	{
		return true;
	}
	if ((leader->GetCircuitDef()->IsAbleToFly() && unit->GetCircuitDef()->IsAbleToFly()) ||
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
	highestRange = std::max(highestRange, unit->GetCircuitDef()->GetLosRadius());

	minPower += unit->GetCircuitDef()->GetPower() / MIN_POWER_DIV;

	if (unit->GetCircuitDef()->IsRoleSupport()) {
		unit->PushBack(new CSupportAction(unit));
	}

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	ITravelAction* travelAction;
	if (unit->GetCircuitDef()->IsAttrMelee()) {
		travelAction = new CMoveAction(unit, squareSize);
	} else {
		travelAction = new CFightAction(unit, squareSize);
	}
	unit->PushBack(travelAction);
	travelAction->SetActive(false);
}

void CAttackTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);
	if ((attackPower <= minPower) || (leader == nullptr)) {
		manager->AbortTask(this);
	} else {
		highestRange = std::max(highestRange, leader->GetCircuitDef()->GetLosRadius());
	}
}

void CAttackTask::Execute(CCircuitUnit* unit)
{
	if (isRegroup || isAttack) {
		return;
	}
	if (!pPath->empty()) {
		ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
		travelAction->SetPath(pPath, lowestSpeed);
		travelAction->SetActive(true);
	}
}

void CAttackTask::Update()
{
	++updCount;

	/*
	 * Merge tasks if possible
	 */
	if (updCount % 32 == 1) {
		ISquadTask* task = GetMergeTask();
		if (task != nullptr) {
			task->Merge(this);
			units.clear();
			// TODO: Deal with cowards?
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
				const AIFloat3& pos = utils::get_radial_pos(groupPos, SQUARE_SIZE * 8);
				TRY_UNIT(circuit, unit,
					unit->GetUnit()->MoveTo(groupPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame);
					unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);
					unit->GetUnit()->PatrolTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, frame);
				)

				ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
				travelAction->SetActive(false);
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
					ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
					travelAction->SetPath(pPath, lowestSpeed);
					travelAction->SetActive(true);
				}
			}
//			ISquadTask::Update();
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
				if (unit->Blocker() != nullptr) {
					continue;  // Do not interrupt current action
				}

				const AIFloat3& pos = utils::get_radial_pos(target->GetPos(), SQUARE_SIZE * 8);
				TRY_UNIT(circuit, unit,
					if (unit->IsJumpReady()) {
						unit->GetUnit()->ExecuteCustomCommand(CMD_JUMP, {pos.x, pos.y, pos.z}, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
						unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, frame + FRAMES_PER_SEC * 60);
					} else if (unit->GetCircuitDef()->IsAttrMelee()) {
						unit->GetUnit()->MoveTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
					} else {
						unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
					}
					unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);
					unit->GetUnit()->ExecuteCustomCommand(CMD_UNIT_SET_TARGET, {(float)target->GetId()});
				)

				ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
				travelAction->SetActive(false);
			}
			return;
		}
	} else {
		CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
		if ((frame > FRAMES_PER_SEC * 300) && (commander != nullptr) &&
			circuit->GetTerrainManager()->CanMoveToPos(leader->GetArea(), commander->GetPos(frame)))
		{
			for (CCircuitUnit* unit : units) {
				unit->Guard(commander, frame + FRAMES_PER_SEC * 60);

				ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
				travelAction->SetActive(false);
			}
			return;
		}
	}
	if (pPath->empty()) {  // should never happen
		for (CCircuitUnit* unit : units) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
				unit->GetUnit()->SetWantedMaxSpeed(lowestSpeed);
			)

			ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
			travelAction->SetActive(false);
		}
	} else {
		for (CCircuitUnit* unit : units) {
			ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
			travelAction->SetPath(pPath, lowestSpeed);
			travelAction->SetActive(true);
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

void CAttackTask::Merge(ISquadTask* task)
{
	ISquadTask::Merge(task);

	minPower += task->GetAttackPower() / MIN_POWER_DIV;
}

void CAttackTask::FindTarget()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = leader->GetPos(circuit->GetLastFrame());
	STerrainMapArea* area = leader->GetArea();
	CCircuitDef* cdef = leader->GetCircuitDef();
	const float speed = SQUARE(highestSpeed);
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float power = attackPower * 0.8f;

	CEnemyUnit* bestTarget = nullptr;
	float minSqDist = std::numeric_limits<float>::max();

	threatMap->SetThreatType(leader);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() || (enemy->GetTasks().size() > 1) ||
			(power <= threatMap->GetThreatAt(enemy->GetPos())) ||
			!terrainManager->CanMoveToPos(area, enemy->GetPos()) ||
			(!cdef->HasAntiWater() && (enemy->GetPos().y < -SQUARE_SIZE * 5)) ||
			(enemy->GetUnit()->GetVel().SqLength2D() > speed))
		{
			continue;
		}

		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			if (((edef->GetCategory() & canTargetCat) == 0) || ((edef->GetCategory() & noChaseCat) != 0) ||
				(enemy->GetUnit()->IsBeingBuilt()))
			{
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
	pathfinder->MakePath(*pPath, startPos, endPos, pathfinder->GetSquareSize(), power);
}

} // namespace circuit
