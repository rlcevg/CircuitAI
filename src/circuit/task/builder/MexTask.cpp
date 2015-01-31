/*
 * MexTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/MexTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "static/MetalManager.h"
#include "module/EconomyManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Unit.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBMexTask::CBMexTask(CCircuitAI* circuit, Priority priority,
					 UnitDef* buildDef, const AIFloat3& position,
					 BuildType type, float cost, int timeout) :
		IBuilderTask(circuit, priority, buildDef, position, BuildType::MEX, cost, timeout)
{
}

CBMexTask::~CBMexTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBMexTask::Execute(CCircuitUnit* unit)
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
	if (buildPos != -RgtVector) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildDef, buildPos, facing)) {
			u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			return;
		} else {
			circuit->GetMetalManager()->SetOpenSpot(buildPos, true);
		}
	}

	buildPos = circuit->GetEconomyManager()->FindBuildPos(unit);
	if (buildPos != -RgtVector) {
		circuit->GetMetalManager()->SetOpenSpot(buildPos, false);
		u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
	} else {
		// Fallback to Guard/Assist/Patrol
		unit->GetManager()->FallbackTask(unit);
	}
}

} // namespace circuit
