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
#include "unit/action/MoveAction.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CAttackTask::CAttackTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::ATTACK)
		, lowestRange(std::numeric_limits<float>::max())
		, highestRange(.0f)
		, lowestSpeed(std::numeric_limits<float>::max())
		, highestSpeed(.0f)
		, leader(nullptr)
		, minPower(.0f)
		, isRegroup(false)
		, isAttack(false)
{
}

CAttackTask::~CAttackTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CAttackTask::CanAssignTo(CCircuitUnit* unit)
{
	return false;
}

void CAttackTask::AssignTo(CCircuitUnit* unit)
{
	IFighterTask::AssignTo(unit);

	if (leader == nullptr) {
		lowestRange  = unit->GetCircuitDef()->GetMaxRange();
		highestRange = unit->GetCircuitDef()->GetMaxRange();
		lowestSpeed  = unit->GetCircuitDef()->GetSpeed();
		highestSpeed = unit->GetCircuitDef()->GetSpeed();
		leader = unit;
	} else {
		lowestRange  = std::min(lowestRange,  unit->GetCircuitDef()->GetMaxRange());
		highestRange = std::max(highestRange, unit->GetCircuitDef()->GetMaxRange());
		lowestSpeed  = std::min(lowestSpeed,  unit->GetCircuitDef()->GetSpeed());
		highestSpeed = std::max(highestSpeed, unit->GetCircuitDef()->GetSpeed());
		if (leader->GetArea() == nullptr) {
			leader = unit;
		} else if ((unit->GetArea() != nullptr) && (unit->GetArea()->percentOfMap < leader->GetArea()->percentOfMap)) {
			leader = unit;
		}
	}

	minPower += unit->GetCircuitDef()->GetPower() / 4;

//	unit->PushBack(new CMoveAction(unit));
}

void CAttackTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);

	if (attackPower < minPower) {
		manager->AbortTask(this);
		return;
	}

	lowestRange  = std::numeric_limits<float>::max();
	highestRange = .0f;
	lowestSpeed  = std::numeric_limits<float>::max();
	highestSpeed = .0f;
	leader = *units.begin();
	for (CCircuitUnit* ass : units) {
		lowestRange  = std::min(lowestRange,  ass->GetCircuitDef()->GetMaxRange());
		highestRange = std::max(highestRange, ass->GetCircuitDef()->GetMaxRange());
		lowestSpeed  = std::min(lowestSpeed,  ass->GetCircuitDef()->GetSpeed());
		highestSpeed = std::max(highestSpeed, ass->GetCircuitDef()->GetSpeed());
		if (leader->GetArea() == nullptr) {
			leader = ass;
		} else if ((ass->GetArea() != nullptr) && (ass->GetArea()->percentOfMap < leader->GetArea()->percentOfMap)) {
			leader = ass;
		}
	}
}

void CAttackTask::Execute(CCircuitUnit* unit)
{
	Execute(unit, false);
}

void CAttackTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	if (!isAttack && (updCount % 16 == 0)) {
		std::vector<CCircuitUnit*> veterans;
		veterans.reserve(units.size());
		CTerrainManager* terrainManager = circuit->GetTerrainManager();;
		AIFloat3 groupPos(ZeroVector);
		for (CCircuitUnit* unit : units) {
			AIFloat3 pos = unit->GetPos(frame);
			terrainManager->CorrectPosition(pos);
			if (terrainManager->CanMoveToPos(unit->GetArea(), pos)) {
				groupPos += pos;
				veterans.push_back(unit);
			}
		}
		groupPos /= veterans.size();

		// find the unit closest to the center (since the actual center might be unreachable)
		float sqMinDist = std::numeric_limits<float>::max();
		float sqMaxDist = .0f;
		CCircuitUnit* closestUnit = *units.begin();
		for (CCircuitUnit* unit : veterans) {
			const float sqDist = groupPos.SqDistance2D(unit->GetPos(frame));
			if (sqDist < sqMinDist) {
				sqMinDist = sqDist;
				closestUnit = unit;
			}
			if (sqDist > sqMaxDist) {
				sqMaxDist = sqDist;
			}
		}
		groupPos = closestUnit->GetPos(frame);

		// have to regroup?
		const float regroupDist = std::max<float>(SQUARE_SIZE * 4 * veterans.size(), highestRange);
		if (sqMaxDist > regroupDist * regroupDist) {
			isRegroup = true;
			for (CCircuitUnit* unit : units) {
				unit->GetUnit()->MoveTo(groupPos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
				unit->GetUnit()->SetWantedMaxSpeed(MAX_SPEED);
			}
		} else {
			isRegroup = false;
		}
	}

	if (isRegroup) {
		return;
	}

	bool isExecute = (++updCount % 4 == 0);
	if (!isExecute) {
		IFighterTask::Update();
		for (CCircuitUnit* unit : units) {
			isExecute |= unit->IsForceExecute();
		}
	}
	if (isExecute) {
		Execute(leader, true);
		const float sqHighestRange = highestRange * highestRange;
		for (CCircuitUnit* unit : units) {
			if (unit == leader) {
				continue;
			}
			const float sqDist = position.SqDistance2D(unit->GetPos(frame));
			if (target != nullptr) {
				const float range = unit->GetUnit()->GetMaxRange();
				if (sqDist < range * range) {
					unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
					unit->GetUnit()->SetWantedMaxSpeed(MAX_SPEED);
					isAttack = true;
					continue;
				}
			} else {
				// Guard commander
				CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
				if ((frame > FRAMES_PER_SEC * 300) && (commander != nullptr) &&
					circuit->GetTerrainManager()->CanMoveToPos(commander->GetArea(), commander->GetPos(frame)))
				{
					unit->Guard(commander, frame + FRAMES_PER_SEC * 60);
					isAttack = false;
					continue;
				}
			}
			unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
			unit->GetUnit()->SetWantedMaxSpeed((sqDist > sqHighestRange) ? lowestSpeed : MAX_SPEED);
			isAttack = false;
		}
	}
}

// FIXME: DEBUG
void CAttackTask::OnUnitIdle(CCircuitUnit* unit)
{
	IFighterTask::OnUnitIdle(unit);

	if (units.find(unit) != units.end()) RemoveAssignee(unit);
}
// FIXME: DEBUG

void CAttackTask::Execute(CCircuitUnit* unit, bool isUpdating)
{
	if ((units.size() > 1) && !isUpdating) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	int frame = circuit->GetLastFrame();

	float minSqDist;
	FindTarget(unit, minSqDist);

	if (target == nullptr) {
		if (!isUpdating) {
			// Guard commander
			CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
			if ((frame > FRAMES_PER_SEC * 300) && (commander != nullptr) &&
				terrainManager->CanMoveToPos(commander->GetArea(), commander->GetPos(frame)))
			{
				unit->Guard(commander, frame + FRAMES_PER_SEC * 60);
				isAttack = false;
				return;
			}

			float x = rand() % (terrainManager->GetTerrainWidth() + 1);
			float z = rand() % (terrainManager->GetTerrainHeight() + 1);
			position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
		}
		minSqDist = position.SqDistance2D(unit->GetPos(frame));
	} else {
		position = target->GetPos();
		const float range = unit->GetUnit()->GetMaxRange();
		if (minSqDist < range * range) {
			unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
			unit->GetUnit()->SetWantedMaxSpeed(MAX_SPEED);
			isAttack = true;
			return;
		}
	}
	unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
	unit->GetUnit()->SetWantedMaxSpeed((minSqDist > highestRange * highestRange) ? lowestSpeed : MAX_SPEED);
	isAttack = false;
}

void CAttackTask::FindTarget(CCircuitUnit* unit, float& minSqDist)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	STerrainMapArea* area = unit->GetArea();
	CCircuitDef* cdef = unit->GetCircuitDef();
	int canTargetCat = cdef->GetTargetCategory();
	int noChaseCat = cdef->GetNoChaseCategory();

	target = nullptr;
	minSqDist = std::numeric_limits<float>::max();

	threatMap->SetThreatType(unit);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() || (threatMap->GetThreatAt(enemy->GetPos()) >= attackPower) ||
			!terrainManager->CanMoveToPos(area, enemy->GetPos()))
		{
			continue;
		}
		if (!cdef->HasAntiWater() && (enemy->GetPos().y < -SQUARE_SIZE * 5)) {
			continue;
		}
		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
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
}

} // namespace circuit
