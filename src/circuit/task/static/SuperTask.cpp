/*
 * SuperTask.cpp
 *
 *  Created on: Aug 12, 2016
 *      Author: rlcevg
 */

#include "task/static/SuperTask.h"
#include "task/TaskManager.h"
#include "module/MilitaryManager.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

#define TARGET_DELAY	(FRAMES_PER_SEC * 20)

CSuperTask::CSuperTask(ITaskManager* mgr)
		: IFighterTask(mgr, IFighterTask::FightType::SUPER)
		, targetFrame(0)
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
	if (targetFrame + TARGET_DELAY > frame) {
		return;
	}

	CCircuitUnit* unit = *units.begin();
	const float maxSqRange = SQUARE(unit->GetCircuitDef()->GetMaxRange());
	float maxCost = 0.f;
	int groupIdx = -1;
	const std::vector<CMilitaryManager::SEnemyGroup>& groups = circuit->GetMilitaryManager()->GetEnemyGroups();
	for (unsigned i = 0; i < groups.size(); ++i) {
		const CMilitaryManager::SEnemyGroup& group = groups[i];
		if ((maxCost < group.cost) && (position.SqDistance2D(group.pos) < maxSqRange)) {
			maxCost = group.cost;
			groupIdx = i;
		}
	}
	const float scale = unit->GetCircuitDef()->IsAttrStock() ? 1.0f : 0.1f;
	if (maxCost < unit->GetCircuitDef()->GetCost() * scale) {
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
	}
}

} // namespace circuit
