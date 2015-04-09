/*
 * NanoTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/NanoTask.h"
#include "task/TaskManager.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "UnitDef.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBNanoTask::CBNanoTask(ITaskManager* mgr, Priority priority,
					   UnitDef* buildDef, const AIFloat3& position,
					   float cost, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, BuildType::NANO, cost, timeout)
{
}

CBNanoTask::~CBNanoTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBNanoTask::Execute(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();

	std::vector<float> params;
	params.push_back(static_cast<float>(priority));
	u->ExecuteCustomCommand(CMD_PRIORITY, params);

	if (target != nullptr) {
		Unit* tu = target->GetUnit();
		u->Build(target->GetDef(), tu->GetPos(), tu->GetBuildingFacing(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
		return;
	}
	CCircuitAI* circuit = manager->GetCircuit();
	if (buildPos != -RgtVector) {
		facing = FindFacing(buildDef, buildPos);
		if (circuit->GetMap()->IsPossibleToBuildAt(buildDef, buildPos, facing)) {
			u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			return;
		} else {
			circuit->GetTerrainManager()->RemoveBlocker(buildDef, buildPos, facing);
		}
	}

	// Alter/randomize position
	AIFloat3 offset((float)rand() / RAND_MAX - 0.5f, 0.0f, (float)rand() / RAND_MAX - 0.5f);
	AIFloat3 pos = position + offset * SQUARE_SIZE * 16;

	CTerrainManager* terrain = circuit->GetTerrainManager();
	CTerrainManager::TerrainPredicate predicate = [terrain, unit](const AIFloat3& p) {
		return terrain->CanBuildAt(unit, p);
	};
	float searchRadius = buildDef->GetBuildDistance();
	facing = FindFacing(buildDef, pos);
	buildPos = terrain->FindBuildSite(buildDef, pos, searchRadius, facing, predicate);

	if (buildPos != -RgtVector) {
		circuit->GetTerrainManager()->AddBlocker(buildDef, buildPos, facing);
		u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

} // namespace circuit
