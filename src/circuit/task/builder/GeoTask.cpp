/*
 * GeoTask.cpp
 *
 *  Created on: Jun 25, 2021
 *      Author: rlcevg
 */

#include "task/builder/GeoTask.h"
#include "module/UnitModule.h"
#include "module/EconomyManager.h"
#include "resource/EnergyManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CBGeoTask::CBGeoTask(IUnitModule* mgr, Priority priority,
					 CCircuitDef* buildDef, int spotId, const AIFloat3& position,
					 SResource cost, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::GEO, cost, 0.f, timeout)
		, spotId(spotId)
{
	SetBuildPos(position);
	manager->GetCircuit()->GetEconomyManager()->SetOpenGeoSpot(spotId, false);
}

CBGeoTask::CBGeoTask(IUnitModule* mgr)
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
		if (spotId >= 0) {  // for broken Load
			manager->GetCircuit()->GetEconomyManager()->SetOpenGeoSpot(spotId, true);
		}
		IBuilderTask::SetBuildPos(-RgtVector);
	}
}

bool CBGeoTask::Execute(CCircuitUnit* unit)
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
	if (circuit->GetMap()->IsPossibleToBuildAt(buildDef->GetDef(), buildPos, facing)) {
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

void CBGeoTask::SetBuildPos(const AIFloat3& pos)
{
	FindFacing(pos);
	IBuilderTask::SetBuildPos(pos);
}

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, spotId);

bool CBGeoTask::Load(std::istream& is)
{
	IBuilderTask::Load(is);
	SERIALIZE(is, read)

	CCircuitAI* circuit = manager->GetCircuit();
	// TODO: Instead of IsSpotValid check IsPossibleToBuildAt
	if (circuit->GetEnergyManager()->IsSpotValid(spotId, GetPosition())) {
		spotId = -1;
#ifdef DEBUG_SAVELOAD
		manager->GetCircuit()->LOG("%s | spotId=%i", __PRETTY_FUNCTION__, spotId);
#endif
		return false;
	}
	circuit->GetEconomyManager()->SetOpenGeoSpot(spotId, false);
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | spotId=%i", __PRETTY_FUNCTION__, spotId);
#endif
	return true;
}

void CBGeoTask::Save(std::ostream& os) const
{
	IBuilderTask::Save(os);
	SERIALIZE(os, write)
}

} // namespace circuit
