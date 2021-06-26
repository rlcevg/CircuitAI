/*
 * MexUpTask.cpp
 *
 *  Created on: Jun 21, 2021
 *      Author: rlcevg
 */

#include "task/builder/MexUpTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CBMexUpTask::CBMexUpTask(ITaskManager* mgr, Priority priority,
						 CCircuitDef* buildDef, int spotId, const AIFloat3& position,
						 float cost, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::MEXUP, cost, 0.f, timeout)
		, spotId(spotId)
{
	manager->GetCircuit()->GetEconomyManager()->SetUpgradingSpot(spotId, true);
}

CBMexUpTask::~CBMexUpTask()
{
}

void CBMexUpTask::Finish()
{
	IBuilderTask::Finish();

	CCircuitAI* circuit = manager->GetCircuit();
	Resource* metalRes = circuit->GetEconomyManager()->GetMetalRes();
	const float maxRange = buildDef->GetDef()->GetResourceExtractorRange(metalRes);
	CCircuitUnit* oldMex = nullptr;
	const auto& unitIds = circuit->GetCallback()->GetFriendlyUnitIdsIn(buildPos, maxRange);
	float curExtract = buildDef->GetDef()->GetExtractsResource(metalRes);
	for (ICoreUnit::Id unitId : unitIds) {
		CCircuitUnit* curMex = circuit->GetTeamUnit(unitId);
		if ((curMex == nullptr) || !curMex->GetCircuitDef()->IsMex()) {
			continue;
		}
		const float extract = curMex->GetCircuitDef()->GetDef()->GetExtractsResource(metalRes);
		if (curExtract > extract) {
			oldMex = curMex;
			break;
		}
	}
	if (oldMex != nullptr) {
		circuit->GetBuilderManager()->EnqueueReclaim(priority, oldMex);
	}

	// FIXME: Won't work with EnqueueReclaim
	circuit->GetEconomyManager()->SetUpgradingSpot(spotId, false);
}

void CBMexUpTask::Cancel()
{
	IBuilderTask::Cancel();

	manager->GetCircuit()->GetEconomyManager()->SetUpgradingSpot(spotId, false);
}

void CBMexUpTask::Execute(CCircuitUnit* unit)
{
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
		}
	}

	circuit->GetThreatMap()->SetThreatType(unit);

	Resource* metalRes = circuit->GetEconomyManager()->GetMetalRes();
	// FIXME: short on purpose, won't work with EnqueueReclaim() in Finish()
	const float searchRadius = /*buildDef->GetDef()->GetResourceExtractorRange(metalRes) + */SQUARE_SIZE * 6;
	FindBuildSite(unit, position, searchRadius);

	if (utils::is_valid(buildPos)) {
		TRY_UNIT(circuit, unit,
			unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
		)
	} else {
		CCircuitUnit* oldMex = nullptr;
		const auto& unitIds = circuit->GetCallback()->GetFriendlyUnitIdsIn(position, searchRadius);
		for (ICoreUnit::Id unitId : unitIds) {
			CCircuitUnit* curMex = circuit->GetTeamUnit(unitId);
			if (curMex == nullptr) {
				continue;
			}
			if (curMex->GetCircuitDef()->GetDef()->GetExtractsResource(metalRes) > 0.f) {
				oldMex = curMex;
				break;
			}
		}
		if (oldMex != nullptr) {
			state = State::ENGAGE;  // reclaim finished => UnitIdle => build on 2nd try
			TRY_UNIT(circuit, unit,
				unit->CmdReclaimUnit(oldMex, UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
			)
		} else {
			manager->AbortTask(this);
		}
	}
}

void CBMexUpTask::OnUnitIdle(CCircuitUnit* unit)
{
	if (State::ENGAGE == state) {
		state = State::ROAM;
		Execute(unit);
		return;
	}

	IBuilderTask::OnUnitIdle(unit);
}

}
// namespace circuit
