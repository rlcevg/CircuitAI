/*
 * DefendTask.cpp
 *
 *  Created on: Feb 12, 2016
 *      Author: rlcevg
 */

#include "task/fighter/DefendTask.h"
#include "task/TaskManager.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "AISCommands.h"

namespace circuit {

using namespace springai;

CDefendTask::CDefendTask(ITaskManager* mgr, const AIFloat3& position, float radius,
						 FightType check, FightType promote, float maxPower, float powerMod)
		: ISquadTask(mgr, FightType::DEFEND, powerMod)
		, radius(radius)
		, check(check)
		, promote(promote)
		, maxPower(maxPower * powerMod)
{
	this->position = position;
}

CDefendTask::~CDefendTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CDefendTask::CanAssignTo(CCircuitUnit* unit) const
{
	return (attackPower < maxPower) && (static_cast<CDefendTask*>(unit->GetTask())->GetPromote() == promote);
}

void CDefendTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);
	if (leader == nullptr) {
		manager->AbortTask(this);
	}
}

void CDefendTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	AIFloat3 pos = utils::get_radial_pos(position, SQUARE_SIZE * 32);
	CTerrainManager::CorrectPosition(pos);
	pos = terrainManager->FindBuildSite(unit->GetCircuitDef(), pos, 300.0f, UNIT_COMMAND_BUILD_NO_FACING);

	TRY_UNIT(circuit, unit,
		unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
		unit->GetUnit()->ExecuteCustomCommand(CMD_WANTED_SPEED, {NO_SPEED_LIMIT});
	)
}

void CDefendTask::Update()
{
	++updCount;

	/*
	 * Merge tasks if possible
	 */
	if (updCount % 32 == 1) {
		CMilitaryManager* militaryManager = static_cast<CMilitaryManager*>(manager);
		if ((attackPower >= maxPower) || !militaryManager->GetTasks(check).empty()) {
			IFighterTask* task = militaryManager->EnqueueTask(promote);
			decltype(units) tmpUnits = units;
			for (CCircuitUnit* unit : tmpUnits) {
				manager->AssignTask(unit, task);
			}
//			manager->DoneTask(this);  // NOTE: RemoveAssignee() will abort task
			return;
		}

		ISquadTask* task = GetMergeTask();
		if (task != nullptr) {
			task->Merge(this);
			units.clear();
			manager->AbortTask(this);
			return;
		}
	}

	/*
	 * No regroup
	 */
	bool isExecute = (updCount % 8 == 2);
	if (!isExecute) {
		for (CCircuitUnit* unit : units) {
			isExecute |= unit->IsForceExecute();
		}
		if (!isExecute) {
			return;
		}
	} else {
		ISquadTask::Update();
		if (leader == nullptr) {  // task aborted
			return;
		}
	}

	/*
	 * Update target
	 */
	FindTarget();

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame() + FRAMES_PER_SEC * 60;
	state = State::ROAM;
	if (target != nullptr) {
		state = State::ENGAGE;
		for (CCircuitUnit* unit : units) {
			unit->Attack(target->GetPos(), frame);
		}
	} else {
		for (CCircuitUnit* unit : units) {
			AIFloat3 pos = utils::get_radial_pos(position, SQUARE_SIZE * 32);
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame);
				unit->GetUnit()->ExecuteCustomCommand(CMD_WANTED_SPEED, {NO_SPEED_LIMIT});
			)
		}
	}
}

void CDefendTask::Merge(ISquadTask* task)
{
	const std::set<CCircuitUnit*>& rookies = task->GetAssignees();
	for (CCircuitUnit* unit : rookies) {
		unit->SetTask(this);
	}
	units.insert(rookies.begin(), rookies.end());
	maxPower = std::max(maxPower, static_cast<CDefendTask*>(task)->GetMaxPower());
	attackPower += task->GetAttackPower();
	const std::set<CCircuitUnit*>& sh = task->GetShields();
	shields.insert(sh.begin(), sh.end());
}

void CDefendTask::FindTarget()
{
	CCircuitAI* circuit = manager->GetCircuit();
	auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(GetPosition(), radius));
	if (enemies.empty()) {
		enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(circuit->GetSetupManager()->GetBasePos(), radius));
		if (enemies.empty()) {
			SetTarget(nullptr);
			return;
		}
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
}

} // namespace circuit
