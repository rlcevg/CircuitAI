/*
 * NanoTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/NanoTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "resource/MetalManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringMap.h"

#include "AISCommands.h"

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
		return;
	}
	if (utils::is_valid(buildPos)) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildDef->GetDef(), buildPos, facing)) {
			TRY_UNIT(circuit, unit,
				unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
			)
			return;
//		} else {
//			SetBuildPos(-RgtVector);
		}
	}

	// Alter/randomize position
	AIFloat3 pos = (shake > .0f) ? utils::get_near_pos(position, shake) : position;

	circuit->GetThreatMap()->SetThreatType(unit);
	float searchRadius = buildDef->GetBuildDistance();
	FindBuildSite(unit, pos, searchRadius);

	if (utils::is_valid(buildPos)) {
		TRY_UNIT(circuit, unit,
			unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
		)
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

} // namespace circuit
