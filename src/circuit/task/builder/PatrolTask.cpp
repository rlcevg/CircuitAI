/*
 * PatrolTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/PatrolTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "Unit.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBPatrolTask::CBPatrolTask(CCircuitAI* circuit, Priority priority,
						   UnitDef* buildDef, const AIFloat3& position,
						   BuildType type, float cost, int timeout) :
		IBuilderTask(circuit, priority, buildDef, position, BuildType::PATROL, cost, timeout)
{
}

CBPatrolTask::~CBPatrolTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBPatrolTask::RemoveAssignee(CCircuitUnit* unit)
{
	unit->GetManager()->SpecialCleanUp(unit);

	IBuilderTask::RemoveAssignee(unit);
}

void CBPatrolTask::MarkCompleted()
{
	for (auto unit : units) {
		unit->GetManager()->SpecialCleanUp(unit);
	}

	IBuilderTask::MarkCompleted();
}

void CBPatrolTask::Execute(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();

	std::vector<float> params;
	params.push_back(0.0f);
	u->ExecuteCustomCommand(CMD_PRIORITY, params);

	const float size = SQUARE_SIZE * 10;
	CTerrainManager* terrain = circuit->GetTerrainManager();
	AIFloat3 pos = position;
	pos.x += (pos.x > terrain->GetTerrainWidth() / 2) ? -size : size;
	pos.z += (pos.z > terrain->GetTerrainHeight() / 2) ? -size : size;
	u->PatrolTo(pos);

	unit->GetManager()->SpecialProcess(unit);
}

} // namespace circuit
