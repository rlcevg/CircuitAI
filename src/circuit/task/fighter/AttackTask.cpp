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
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CAttackTask::CAttackTask(ITaskManager* mgr)
		: ISquadTask(mgr, FightType::ATTACK)
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

bool CAttackTask::CanAssignTo(CCircuitUnit* unit)
{
	if (leader == nullptr) {
		return true;
	}
	int frame = manager->GetCircuit()->GetLastFrame();
	return leader->GetPos(frame).SqDistance2D(unit->GetPos(frame)) < SQUARE(1000.0f);
}

void CAttackTask::Update()
{
	if (IsRegroup()) {
		return;
	}

	bool isExecute = (updCount++ % 4 == 0);
	if (!isExecute) {
		for (CCircuitUnit* unit : units) {
			isExecute |= unit->IsForceExecute();
		}
	}
	if (!isExecute) {
		IFighterTask::Update();
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	FindTarget(leader);

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
				unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
				unit->GetUnit()->SetWantedMaxSpeed(MAX_SPEED);
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
			}
			return;
		}
	}
	for (CCircuitUnit* unit : units) {
		unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		unit->GetUnit()->SetWantedMaxSpeed(lowestSpeed);
	}
}

void CAttackTask::OnUnitIdle(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (position.SqDistance2D(leader->GetPos(circuit->GetLastFrame())) < SQUARE(lowestRange)) {
		float x = rand() % (circuit->GetTerrainManager()->GetTerrainWidth() + 1);
		float z = rand() % (circuit->GetTerrainManager()->GetTerrainHeight() + 1);
		position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	}
}

void CAttackTask::FindTarget(CCircuitUnit* unit)
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
	float minSqDist = std::numeric_limits<float>::max();

	threatMap->SetThreatType(unit);
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
			if ((threat * 8.0f <= attackPower) && edef->IsMobile()) {
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
}

} // namespace circuit
