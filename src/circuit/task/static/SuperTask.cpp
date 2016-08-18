/*
 * SuperTask.cpp
 *
 *  Created on: Aug 12, 2016
 *      Author: rlcevg
 */

#include "task/static/SuperTask.h"
#include "task/fighter/SquadTask.h"
#include "task/TaskManager.h"
#include "module/MilitaryManager.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

#define TARGET_DELAY	(FRAMES_PER_SEC * 10)
// NOTE: Nuke flies to target after attack command for about 30 seconds
#define STOCK_DELAY		(FRAMES_PER_SEC * 40)

CSuperTask::CSuperTask(ITaskManager* mgr)
		: IFighterTask(mgr, IFighterTask::FightType::SUPER)
		, targetFrame(0)
		, isAttack(false)
{
}

CSuperTask::~CSuperTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CSuperTask::CanAssignTo(CCircuitUnit* unit) const
{
	return false;
}

void CSuperTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CSuperTask::Execute(CCircuitUnit* unit)
{
	int frame = manager->GetCircuit()->GetLastFrame();
	targetFrame = frame - TARGET_DELAY;
	position = unit->GetPos(frame);
}

void CSuperTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	CCircuitUnit* unit = *units.begin();
	if (unit->GetCircuitDef()->IsAttrHoldFire()) {
		if (targetFrame + STOCK_DELAY > frame) {
			if (isAttack && (targetFrame + TARGET_DELAY <= frame)) {
				TRY_UNIT(circuit, unit,
					unit->GetUnit()->Stop();
				)
				isAttack = false;
			}
			return;
		}
	} else if (targetFrame + TARGET_DELAY > frame) {
		return;
	}

	CMilitaryManager* militaryManager = circuit->GetMilitaryManager();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float maxSqRange = SQUARE(cdef->GetMaxRange());
	const float sqAoe = SQUARE(cdef->GetAoe() * 1.25f);
	float cost = 0.f;
	int groupIdx = -1;
	const std::array<const std::set<IFighterTask*>*, 3> avoidTasks = {  // NOTE: ISquadTask only
		&militaryManager->GetTasks(IFighterTask::FightType::ATTACK),
		&militaryManager->GetTasks(IFighterTask::FightType::AH),
		&militaryManager->GetTasks(IFighterTask::FightType::AA),
	};
	const std::vector<CMilitaryManager::SEnemyGroup>& groups = militaryManager->GetEnemyGroups();
	for (unsigned i = 0; i < groups.size(); ++i) {
		const CMilitaryManager::SEnemyGroup& group = groups[i];
		if ((cost >= group.cost) || (position.SqDistance2D(group.pos) >= maxSqRange)) {
			continue;
		}
		bool isAvoid = true;
		for (const std::set<IFighterTask*>* tasks : avoidTasks) {
			for (const IFighterTask* task : *tasks) {
				const AIFloat3& leaderPos = static_cast<const ISquadTask*>(task)->GetLeaderPos(frame);
				if (leaderPos.SqDistance2D(group.pos) < sqAoe) {
					isAvoid = false;
					break;
				}
			}
			if (!isAvoid) {
				break;
			}
		}
		if (isAvoid) {
			cost = group.cost;
			groupIdx = i;
		}
	}
	const float maxCost = cdef->IsAttrStock() ? cdef->GetStockCost() : cdef->GetCost() * 0.1f;
	if (cost < maxCost) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Stop();
		)
		SetTarget(nullptr);
		targetFrame = frame;
		return;
	}

	const AIFloat3& grPos = groups[groupIdx].pos;
	CEnemyUnit* bestTarget = nullptr;
	float minSqDist = std::numeric_limits<float>::max();
	for (const CCircuitUnit::Id eId : groups[groupIdx].units) {
		CEnemyUnit* enemy = circuit->GetEnemyUnit(eId);
		if (enemy == nullptr) {
			continue;
		}
		const float sqDist = grPos.SqDistance2D(enemy->GetPos());
		if ((minSqDist > sqDist) && (position.SqDistance2D(enemy->GetPos()) < maxSqRange)) {
			minSqDist = sqDist;
			bestTarget = enemy;
		}
	}
	SetTarget(bestTarget);
	if (target != nullptr) {
		const AIFloat3& pos = target->GetPos();
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->ExecuteCustomCommand(CMD_ATTACK_GROUND, {pos.x, pos.y, pos.z},
												  UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		)
		targetFrame = frame;
		isAttack = true;
	}
}

} // namespace circuit
