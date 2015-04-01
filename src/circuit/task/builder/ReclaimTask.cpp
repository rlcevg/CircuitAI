/*
 * ReclaimTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/ReclaimTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Unit.h"

namespace circuit {

using namespace springai;

CBReclaimTask::CBReclaimTask(ITaskManager* mgr, Priority priority,
							 const AIFloat3& position,
							 float cost, int timeout, float radius) :
		IBuilderTask(mgr, priority, nullptr, position, BuildType::RECLAIM, cost, timeout),
		radius(radius)
{
}

CBReclaimTask::~CBReclaimTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBReclaimTask::RemoveAssignee(CCircuitUnit* unit)
{
	// Unregister from timeout processor
	manager->SpecialCleanUp(unit);

	IBuilderTask::RemoveAssignee(unit);
}

void CBReclaimTask::Execute(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();

	std::vector<float> params;
	params.push_back(static_cast<float>(priority));
	u->ExecuteCustomCommand(CMD_PRIORITY, params);

	if (target == nullptr) {
		AIFloat3 pos;
		float reclRadius;
		if ((position == -RgtVector) || (radius == .0f)) {
			CTerrainManager* terrain = manager->GetCircuit()->GetTerrainManager();
			float width = terrain->GetTerrainWidth() / 2;
			float height = terrain->GetTerrainHeight() / 2;
			pos = AIFloat3(width, 0, height);
			reclRadius = sqrtf(width * width + height * height);
		} else {
			pos = position;
			reclRadius = radius;
		}
		u->ReclaimInArea(pos, reclRadius, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
	} else {
		u->ReclaimUnit(target->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
	}

	// Register unit to process timeout if set
	manager->SpecialProcess(unit);
}

void CBReclaimTask::Update()
{
	if (manager->GetCircuit()->GetEconomyManager()->IsMetalFull()) {
		manager->AbortTask(this);
	}
}

void CBReclaimTask::Close(bool done)
{
	for (auto unit : units) {
		// Unregister from timeout processor
		manager->SpecialCleanUp(unit);
	}

	IBuilderTask::Close(done);
}

void CBReclaimTask::Cancel()
{
}

} // namespace circuit
