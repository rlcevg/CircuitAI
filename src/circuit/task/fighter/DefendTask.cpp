/*
 * DefendTask.cpp
 *
 *  Created on: Feb 12, 2016
 *      Author: rlcevg
 */

#include "task/fighter/DefendTask.h"
#include "task/TaskManager.h"
#include "terrain/TerrainManager.h"
#include "module/MilitaryManager.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "AISCommands.h"

namespace circuit {

using namespace springai;

CDefendTask::CDefendTask(ITaskManager* mgr, const AIFloat3& position, unsigned maxSize)
		: ISquadTask(mgr, FightType::DEFEND)
		, maxSize(maxSize)
{
	this->position = position;
}

CDefendTask::~CDefendTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CDefendTask::CanAssignTo(CCircuitUnit* unit) const
{
	return units.size() < maxSize;
}

void CDefendTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CDefendTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	AIFloat3 pos = terrainManager->FindBuildSite(unit->GetCircuitDef(), position, 300.0f, UNIT_COMMAND_BUILD_NO_FACING);

	TRY_UNIT(circuit, unit,
		unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
		unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);
	)
}

void CDefendTask::Update()
{
	++updCount;

	/*
	 * Merge tasks if possible
	 */
	if (updCount % 32 == 1) {
		if (units.size() >= maxSize) {
			IFighterTask* task = static_cast<CMilitaryManager*>(manager)->EnqueueTask(IFighterTask::FightType::ATTACK);
			decltype(units) tmpUnits = units;
			for (CCircuitUnit* ass : tmpUnits) {
				manager->AssignTask(ass, task);
			}
//			manager->DoneTask(this);  // NOTE: RemoveAssignee will abort task
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
	 * Regroup if required
	 */
	bool mustRegroup = IsMustRegroup();
	if (isRegroup) {
		if (mustRegroup) {
			CCircuitAI* circuit = manager->GetCircuit();
			int frame = circuit->GetLastFrame() + FRAMES_PER_SEC * 60;
			for (CCircuitUnit* unit : units) {
				TRY_UNIT(circuit, unit,
					unit->GetUnit()->Fight(groupPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame);
					unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);
				)
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
			return;
		}
	}

	/*
	 * Update target
	 */
	FindTarget();

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame() + FRAMES_PER_SEC * 60;
	isAttack = false;
	if (target != nullptr) {
		isAttack = true;
		for (CCircuitUnit* unit : units) {
			const AIFloat3& pos = utils::get_radial_pos(target->GetPos(), SQUARE_SIZE * 8);
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame);
				unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);
			)
		}
	} else {
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		AIFloat3 pos = position;
		terrainManager->CorrectPosition(pos);
		pos = terrainManager->FindBuildSite(leader->GetCircuitDef(), pos, 300.f, UNIT_COMMAND_BUILD_NO_FACING);

		for (CCircuitUnit* unit : units) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame);
				unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);
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
}

void CDefendTask::FindTarget()
{
	CCircuitAI* circuit = manager->GetCircuit();
	auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(GetPosition(), 2000.f));
	if (enemies.empty()) {
		SetTarget(nullptr);
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
}

} // namespace circuit
