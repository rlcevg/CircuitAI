/*
 * PylonTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/PylonTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "resource/EnergyLink.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBPylonTask::CBPylonTask(ITaskManager* mgr, Priority priority,
						 CCircuitDef* buildDef, const AIFloat3& position,
						 CEnergyLink* link, float cost, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, BuildType::PYLON, cost, false, timeout)
		, link(link)
{
	if (link != nullptr) {
		link->SetBeingBuilt(true);
	}
}

CBPylonTask::~CBPylonTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBPylonTask::Execute(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	u->ExecuteCustomCommand(CMD_PRIORITY, {static_cast<float>(priority)});

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	if (target != nullptr) {
		int facing = target->GetUnit()->GetBuildingFacing();
		u->Build(target->GetCircuitDef()->GetUnitDef(), target->GetPos(frame), facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
		return;
	}
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	UnitDef* buildUDef = buildDef->GetUnitDef();
	if (buildPos != -RgtVector) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
			u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
			return;
		} else {
			terrainManager->RemoveBlocker(buildDef, buildPos, facing);
		}
	}

	circuit->GetThreatMap()->SetThreatType(unit);
	const float searchRadius = circuit->GetEconomyManager()->GetPylonRange() * 0.5f;
	FindBuildSite(unit, position, searchRadius);

	if (buildPos != -RgtVector) {
		terrainManager->AddBlocker(buildDef, buildPos, facing);
		u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

void CBPylonTask::Finish()
{
	if (link != nullptr) {
		link->SetBeingBuilt(false);
	}
	manager->GetCircuit()->GetEconomyManager()->UpdatePylonTasks();

	IBuilderTask::Finish();
}

void CBPylonTask::Cancel()
{
	if (link != nullptr) {
		link->SetBeingBuilt(false);
	}

	IBuilderTask::Cancel();
}

} // namespace circuit
