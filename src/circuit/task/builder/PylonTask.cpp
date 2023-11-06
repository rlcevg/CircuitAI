/*
 * PylonTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/PylonTask.h"
#include "map/ThreatMap.h"
#include "module/UnitModule.h"
#include "module/EconomyManager.h"
#include "resource/GridLink.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CBPylonTask::CBPylonTask(IUnitModule* mgr, Priority priority,
						 CCircuitDef* buildDef, const AIFloat3& position,
						 IGridLink* link, SResource cost, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::PYLON, cost, 0.f, timeout)
		, link(link)
{
	if (link != nullptr) {
		link->SetBeingBuilt(true);
	}
}

CBPylonTask::CBPylonTask(IUnitModule* mgr)
		: IBuilderTask(mgr, Type::BUILDER, BuildType::PYLON)
		, link(nullptr)
{
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

bool CBPylonTask::Execute(CCircuitUnit* unit)
{
	executors.insert(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	TRY_UNIT(circuit, unit,
		unit->CmdPriority(ClampPriority());
	)

	const int frame = circuit->GetLastFrame();
	if (target != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->CmdRepair(target, UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
		)
		return true;
	}
	if (utils::is_valid(buildPos)
		&& circuit->GetMap()->IsPossibleToBuildAt(buildDef->GetDef(), buildPos, facing))
	{
		TRY_UNIT(circuit, unit,
			unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
		)
		return true;
	}

	circuit->GetThreatMap()->SetThreatType(unit);
	const float searchRadius = circuit->GetEconomyManager()->GetPylonRange() * 0.5f;
	FindBuildSite(unit, position, searchRadius);

	if (utils::is_valid(buildPos)) {
		TRY_UNIT(circuit, unit,
			unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
		)
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
		return false;
	}
	return true;
}

} // namespace circuit
