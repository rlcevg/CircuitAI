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
#include "terrain/ThreatMap.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBNanoTask::CBNanoTask(ITaskManager* mgr, Priority priority,
					   CCircuitDef* buildDef, const AIFloat3& position,
					   float cost, bool isShake, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, BuildType::NANO, cost, isShake, timeout)
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

	CCircuitAI* circuit = manager->GetCircuit();
	if (target != nullptr) {
		Unit* tu = target->GetUnit();
		u->Build(target->GetCircuitDef()->GetUnitDef(), tu->GetPos(), tu->GetBuildingFacing(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
		return;
	}
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	UnitDef* buildUDef = buildDef->GetUnitDef();
	if (buildPos != -RgtVector) {
		facing = FindFacing(buildDef, buildPos);
		if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
			u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
			return;
		} else {
			terrainManager->RemoveBlocker(buildDef, buildPos, facing);
		}
	}

	// Alter/randomize position
	AIFloat3 pos;
	if (isShake) {
		AIFloat3 offset((float)rand() / RAND_MAX - 0.5f, 0.0f, (float)rand() / RAND_MAX - 0.5f);
		pos = position + offset * SQUARE_SIZE * 16;
	} else {
		pos = position;
	}

	circuit->GetThreatMap()->SetThreatType(unit);
	CTerrainManager::TerrainPredicate predicate = [terrainManager, unit](const AIFloat3& p) {
		return terrainManager->CanBuildAt(unit, p);
	};
	float searchRadius = buildDef->GetBuildDistance();
	facing = FindFacing(buildDef, pos);
	buildPos = terrainManager->FindBuildSite(buildDef, pos, searchRadius, facing, predicate);

	if (buildPos != -RgtVector) {
		terrainManager->AddBlocker(buildDef, buildPos, facing);
		u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

} // namespace circuit
