/*
 * ReclaimTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/ReclaimTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Unit.h"

namespace circuit {

using namespace springai;

CBReclaimTask::CBReclaimTask(CCircuitAI* circuit, Priority priority,
							 UnitDef* buildDef, const AIFloat3& position,
							 float cost, int timeout) :
		IBuilderTask(circuit, priority, buildDef, position, BuildType::RECLAIM, cost, timeout)
{
}

CBReclaimTask::~CBReclaimTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBReclaimTask::RemoveAssignee(CCircuitUnit* unit)
{
	unit->GetManager()->SpecialCleanUp(unit);

	IBuilderTask::RemoveAssignee(unit);
}

void CBReclaimTask::MarkCompleted()
{
	for (auto unit : units) {
		unit->GetManager()->SpecialCleanUp(unit);
	}

	IBuilderTask::MarkCompleted();
}

void CBReclaimTask::Execute(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();

	std::vector<float> params;
	params.push_back(static_cast<float>(priority));
	u->ExecuteCustomCommand(CMD_PRIORITY, params);

	CTerrainManager* terrain = circuit->GetTerrainManager();
	float width = terrain->GetTerrainWidth() / 2;
	float height = terrain->GetTerrainHeight() / 2;
	AIFloat3 pos(width, 0, height);
	float radius = sqrtf(width * width + height * height);
	unit->GetUnit()->ReclaimInArea(pos, radius, UNIT_COMMAND_OPTION_SHIFT_KEY, FRAMES_PER_SEC * 60);

	unit->GetManager()->SpecialProcess(unit);
}

} // namespace circuit
