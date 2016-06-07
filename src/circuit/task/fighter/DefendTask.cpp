/*
 * DefendTask.cpp
 *
 *  Created on: Feb 12, 2016
 *      Author: rlcevg
 */

#include "task/fighter/DefendTask.h"
#include "task/TaskManager.h"
#include "terrain/TerrainManager.h"
#include "module/FactoryManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "AISCommands.h"

namespace circuit {

using namespace springai;

CDefendTask::CDefendTask(ITaskManager* mgr, const AIFloat3& position, float maxPower)
		: ISquadTask(mgr, FightType::DEFEND)
		, maxPower(maxPower)
{
	this->position = position;

	CCircuitAI* circuit = manager->GetCircuit();
	CFactoryManager* factoryManager = circuit->GetFactoryManager();
	AIFloat3 pos = position;
	CCircuitDef* buildDef = factoryManager->GetClosestDef(pos, CCircuitDef::RoleType::RIOT);
	if (buildDef != nullptr) {
		factoryManager->EnqueueTask(CRecruitTask::Priority::HIGH, buildDef, pos,
									CRecruitTask::RecruitType::FIREPOWER, SQUARE(SQUARE_SIZE));
	}
}

CDefendTask::~CDefendTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CDefendTask::CanAssignTo(CCircuitUnit* unit) const
{
	return (attackPower < maxPower) && unit->GetCircuitDef()->IsRoleRiot();
}

void CDefendTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(GetPosition(), 1000.f));
	if (enemies.empty()) {
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		AIFloat3 pos = position;
		terrainManager->CorrectPosition(pos);
		pos = terrainManager->FindBuildSite(unit->GetCircuitDef(), pos, 300.f, UNIT_COMMAND_BUILD_NO_FACING);

		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
			unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);
		)
		return;
	}

	CEnemyUnit* bestTarget = nullptr;
	float minSqDist = std::numeric_limits<float>::max();

	for (Unit* e : enemies) {
		if (e == nullptr) {
			continue;
		}
		CEnemyUnit* enemy = circuit->GetEnemyUnit(e);
		if (enemy != nullptr) {
			float sqDist = GetPosition().SqDistance2D(enemy->GetPos());
			if (minSqDist > sqDist) {
				minSqDist = sqDist;
				bestTarget = enemy;
			}
		}
		delete e;
	}

	SetTarget(bestTarget);
	if (bestTarget != nullptr) {
		const AIFloat3& pos = utils::get_radial_pos(target->GetPos(), SQUARE_SIZE * 8);
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
			unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);
		)
	}
}

void CDefendTask::Update()
{
	if (updCount % 8 == 0) {
		CCircuitAI* circuit = manager->GetCircuit();
		CMilitaryManager* militaryManager = circuit->GetMilitaryManager();
		int cluster = circuit->GetMetalManager()->FindNearestCluster(position);
		if ((cluster >= 0) && militaryManager->HasDefence(cluster)) {
			IUnitTask* task = circuit->GetMilitaryManager()->DelDefendTask(cluster);
			if (task != nullptr) {
				task->GetManager()->DoneTask(task);
			}
			// FIXME: Addg general defend-complete condition?
			return;
		}
	}

	bool isExecute = (++updCount % 4 == 0);
	for (CCircuitUnit* unit : units) {
		if (unit->IsForceExecute() || isExecute) {
			Execute(unit);
		} else {
			ISquadTask::Update();
		}
	}
}

void CDefendTask::Cancel()
{
	manager->GetCircuit()->GetMilitaryManager()->DelDefendTask(position);
}

} // namespace circuit
