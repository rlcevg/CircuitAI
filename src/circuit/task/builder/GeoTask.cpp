/*
 * GeoTask.cpp
 *
 *  Created on: Jun 25, 2021
 *      Author: rlcevg
 */

#include "task/builder/GeoTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CBGeoTask::CBGeoTask(ITaskManager* mgr, Priority priority,
					 CCircuitDef* buildDef, int spotId, const AIFloat3& position,
					 float cost, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::GEO, cost, 0.f, timeout)
		, spotId(spotId)
{
	SetBuildPos(position);
	manager->GetCircuit()->GetEconomyManager()->SetOpenGeoSpot(spotId, false);
}

CBGeoTask::CBGeoTask(ITaskManager* mgr)
		: IBuilderTask(mgr, Type::BUILDER, BuildType::GEO)
		, spotId(-1)
{
}

CBGeoTask::~CBGeoTask()
{
}

void CBGeoTask::Cancel()
{
	if ((target == nullptr) && utils::is_valid(buildPos)) {
		manager->GetCircuit()->GetEconomyManager()->SetOpenGeoSpot(spotId, true);
		IBuilderTask::SetBuildPos(-RgtVector);
	}
}

void CBGeoTask::Execute(CCircuitUnit* unit)
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
	if (circuit->GetMap()->IsPossibleToBuildAt(buildDef->GetDef(), buildPos, facing)) {
		TRY_UNIT(circuit, unit,
			unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
		)
		return;
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

void CBGeoTask::SetBuildPos(const AIFloat3& pos)
{
	FindFacing(pos);
	IBuilderTask::SetBuildPos(pos);
}

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, spotId);

void CBGeoTask::Load(std::istream& is)
{
	IBuilderTask::Load(is);
	SERIALIZE(is, read)

	CCircuitAI* circuit = manager->GetCircuit();
	circuit->GetEconomyManager()->SetOpenGeoSpot(spotId, false);
}

void CBGeoTask::Save(std::ostream& os) const
{
	IBuilderTask::Save(os);
	SERIALIZE(os, write)
}

} // namespace circuit
