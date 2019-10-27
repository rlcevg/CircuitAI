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
					   float cost, float shake, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::NANO, cost, shake, timeout)
{
}

CBNanoTask::~CBNanoTask()
{
}

void CBNanoTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	Unit* u = unit->GetUnit();
	TRY_UNIT(circuit, unit,
		u->ExecuteCustomCommand(CMD_PRIORITY, {ClampPriority()});
	)

	const int frame = circuit->GetLastFrame();
	if (target != nullptr) {
		TRY_UNIT(circuit, unit,
			u->Repair(target->GetUnit(), UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
		)
		return;
	}
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	UnitDef* buildUDef = buildDef->GetDef();
	if (utils::is_valid(buildPos)) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
			TRY_UNIT(circuit, unit,
				u->Build(buildUDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
			)
			return;
		} else {
			terrainManager->DelBlocker(buildDef, buildPos, facing);
		}
	}

	// Alter/randomize position
	AIFloat3 pos = (shake > .0f) ? utils::get_near_pos(position, shake) : position;

	circuit->GetThreatMap()->SetThreatType(unit);
	float searchRadius = buildDef->GetBuildDistance();
	FindBuildSite(unit, pos, searchRadius);

	if (utils::is_valid(buildPos)) {
		terrainManager->AddBlocker(buildDef, buildPos, facing);
		TRY_UNIT(circuit, unit,
			u->Build(buildUDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
		)
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

} // namespace circuit
