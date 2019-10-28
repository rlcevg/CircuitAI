/*
 * PylonTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/PylonTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "resource/GridLink.h"
#include "terrain/ThreatMap.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBPylonTask::CBPylonTask(ITaskManager* mgr, Priority priority,
						 CCircuitDef* buildDef, const AIFloat3& position,
						 IGridLink* link, float cost, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::PYLON, cost, 0.f, timeout)
		, link(link)
{
	if (link != nullptr) {
		link->SetBeingBuilt(true);
	}
}

CBPylonTask::~CBPylonTask()
{
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

void CBPylonTask::Execute(CCircuitUnit* unit)
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
	UnitDef* buildUDef = buildDef->GetDef();
	if (utils::is_valid(buildPos)) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
			TRY_UNIT(circuit, unit,
				u->Build(buildUDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
			)
			return;
//		} else {
//			SetBuildPos(-RgtVector);
		}
	}

	circuit->GetThreatMap()->SetThreatType(unit);
	const float searchRadius = circuit->GetEconomyManager()->GetPylonRange() * 0.5f;
	FindBuildSite(unit, position, searchRadius);

	if (utils::is_valid(buildPos)) {
		TRY_UNIT(circuit, unit,
			u->Build(buildUDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
		)
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

} // namespace circuit
