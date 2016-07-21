/*
 * MexTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/MexTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "module/BuilderManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBMexTask::CBMexTask(ITaskManager* mgr, Priority priority,
					 CCircuitDef* buildDef, const AIFloat3& position,
					 float cost, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, BuildType::MEX, cost, false, timeout)
{
}

CBMexTask::~CBMexTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CBMexTask::CanAssignTo(CCircuitUnit* unit) const
{
	// FIXME: Resume fighter/DefendTask experiment
	return IBuilderTask::CanAssignTo(unit);
	// FIXME: Resume fighter/DefendTask experiment

	if (!IBuilderTask::CanAssignTo(unit)) {
		return false;
	}
	if (unit->GetCircuitDef()->IsAttacker()) {
		return true;
	}
	// TODO: Naked expansion on big maps
	CCircuitAI* circuit = manager->GetCircuit();
	CMilitaryManager* militaryManager = circuit->GetMilitaryManager();
	int cluster = circuit->GetMetalManager()->FindNearestCluster(GetPosition());
	if ((cluster < 0) || militaryManager->HasDefence(cluster)) {
		return true;
	}
	IUnitTask* defend = militaryManager->GetDefendTask(cluster);
	return (defend != nullptr) && !defend->GetAssignees().empty();
}

void CBMexTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	Unit* u = unit->GetUnit();
	TRY_UNIT(circuit, unit,
		u->ExecuteCustomCommand(CMD_PRIORITY, {ClampPriority()});
	)

	int frame = circuit->GetLastFrame();
	if (target != nullptr) {
		int facing = target->GetUnit()->GetBuildingFacing();
		TRY_UNIT(circuit, unit,
			u->Build(target->GetCircuitDef()->GetUnitDef(), target->GetPos(frame), facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
		)
		return;
	}
	CMetalManager* metalManager = circuit->GetMetalManager();
	UnitDef* buildUDef = buildDef->GetUnitDef();
	if (utils::is_valid(buildPos)) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
			TRY_UNIT(circuit, unit,
				u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
			)
			return;
		} else {
			metalManager->SetOpenSpot(buildPos, true);
		}
	}

	// FIXME: Unsafe fallback expansion (mex can be behind enemy lines)
//	const CMetalData::Metals& spots = metalManager->GetSpots();
//	Map* map = circuit->GetMap();
//	CTerrainManager* terrainManager = circuit->GetTerrainManager();
//	circuit->GetThreatMap()->SetThreatType(unit);
//	CMetalData::MetalPredicate predicate = [&spots, metalManager, map, buildUDef, terrainManager, unit](CMetalData::MetalNode const& v) {
//		int index = v.second;
//		return (metalManager->IsOpenSpot(index) &&
//				terrainManager->CanBuildAt(unit, spots[index].position) &&
//				map->IsPossibleToBuildAt(buildUDef, spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
//	};
//	int index = metalManager->FindNearestSpot(position, predicate);
//	buildPos = (index >= 0) ? spots[index].position : AIFloat3(-RgtVector);
//
//	if (utils::is_valid(buildPos)) {
//		metalManager->SetOpenSpot(buildPos, false);
//		TRY_UNIT(circuit, unit,
//			u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
//		)
//	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
//	}
}

void CBMexTask::Finish()
{
	CCircuitAI* circuit = manager->GetCircuit();
//	CMetalManager* metalManager = circuit->GetMetalManager();
//	int index = metalManager->FindNearestCluster(buildPos);
//	if ((index >= 0) && metalManager->IsClusterFinished(index)) {
		circuit->GetMilitaryManager()->MakeDefence(buildPos);
//	}

	CCircuitDef* energyDef = circuit->GetEconomyManager()->GetLowEnergy(buildPos);
	if ((energyDef != nullptr) && energyDef->IsAvailable()) {
		circuit->GetBuilderManager()->EnqueueTask(IBuilderTask::Priority::NORMAL, energyDef, buildPos,
												  IBuilderTask::BuildType::ENERGY, SQUARE_SIZE * 8.0f, true);
	}

//	if (circuit->GetEconomyManager()->GetAvgMetalIncome() > 8.0f) {
//		circuit->GetBuilderManager()->EnqueueTerraform(IBuilderTask::Priority::HIGH, target);
//	}
}

void CBMexTask::Cancel()
{
	if ((target == nullptr) && utils::is_valid(buildPos)) {
		manager->GetCircuit()->GetMetalManager()->SetOpenSpot(buildPos, true);
	}
}

void CBMexTask::OnUnitIdle(CCircuitUnit* unit)
{
	/*
	 * Check if unit is idle because of enemy mex ahead and build turret if so.
	 */
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitDef* def = circuit->GetCircuitDef("corrl");
	float range = def->GetMaxRange();
	float testRange = range + 200.0f;  // 200 elmos
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	if (buildPos.SqDistance2D(pos) < testRange * testRange) {
		int mexDefId = circuit->GetEconomyManager()->GetMexDef()->GetId();
		// TODO: Use internal CCircuitAI::GetEnemyUnits?
		auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(buildPos, SQUARE_SIZE));
		bool blocked = false;
		for (Unit* enemy : enemies) {
			if (enemy == nullptr) {
				continue;
			}
			UnitDef* def = enemy->GetDef();
			int enemyDefId = def->GetUnitDefId();
			delete def;
			if (enemyDefId == mexDefId) {
				blocked = true;
				break;
			}
		}
		utils::free_clear(enemies);
		if (blocked) {
			CBuilderManager* builderManager = circuit->GetBuilderManager();
			IBuilderTask* task = nullptr;
			const float qdist = SQUARE(200.0f);  // 200 elmos
			// TODO: Push tasks into bgi::rtree
			for (IBuilderTask* t : builderManager->GetTasks(IBuilderTask::BuildType::DEFENCE)) {
				if (pos.SqDistance2D(t->GetTaskPos()) < qdist) {
					task = t;
					break;
				}
			}
			if (task == nullptr) {
				AIFloat3 newPos = buildPos - (buildPos - pos).Normalize2D() * range * 0.9f;
				task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, def, newPos, IBuilderTask::BuildType::DEFENCE);
			}
			// TODO: Before BuildTask assign MoveTask(task->GetTaskPos())
			manager->AssignTask(unit, task);
			return;
		}
	}

	IBuilderTask::OnUnitIdle(unit);
}

} // namespace circuit
