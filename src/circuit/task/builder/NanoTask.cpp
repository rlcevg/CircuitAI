/*
 * NanoTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/NanoTask.h"
#include "task/TaskManager.h"
#include "unit/CircuitUnit.h"
#include "resource/ResourceManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "UnitDef.h"
#include "Unit.h"
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

	float searchRadius = buildDef->GetBuildDistance();
	facing = FindFacing(buildDef, position);
	CTerrainManager* terrain = circuit->GetTerrainManager();
	buildPos = terrain->FindBuildSite(buildDef, position, searchRadius, facing);
	if (buildPos == -RgtVector) {
		const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
		const CMetalData::MetalIndices indices = circuit->GetMetalManager()->FindNearestClusters(position, 3);
		for (const int idx : indices) {
			facing = FindFacing(buildDef, clusters[idx].geoCentr);
			buildPos = terrain->FindBuildSite(buildDef, clusters[idx].geoCentr, searchRadius, facing);
			if (buildPos != -RgtVector) {
				break;
			}
		}
	}

	if (buildPos != -RgtVector) {
		circuit->GetTerrainManager()->AddBlocker(buildDef, buildPos, facing);
		u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

} // namespace circuit
