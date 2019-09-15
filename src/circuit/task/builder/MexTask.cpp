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
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::MEX, cost, 0.f, timeout)
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

	const int frame = circuit->GetLastFrame();
	if (target != nullptr) {
		int facing = target->GetUnit()->GetBuildingFacing();
		TRY_UNIT(circuit, unit,
			u->Build(target->GetCircuitDef()->GetUnitDef(), target->GetPos(frame), facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
		)
		return;
	}
	CMetalManager* metalManager = circuit->GetMetalManager();
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	UnitDef* buildUDef = buildDef->GetUnitDef();
	if (utils::is_valid(buildPos)) {
		int index = metalManager->FindNearestSpot(buildPos);
		if (index >= 0) {
			if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
				if ((State::ENGAGE == state) || metalManager->IsOpenSpot(index)) {  // !isFirstTry
					state = State::ENGAGE;  // isFirstTry = false
					metalManager->SetOpenSpot(index, false);
					TRY_UNIT(circuit, unit,
						u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
					)
					return;
				} else {
					economyManager->SetOpenSpot(index, true);
				}
			} else {
				metalManager->SetOpenSpot(index, true);
				economyManager->SetOpenSpot(index, true);
			}
		}
	}

	// NOTE: Unsafe fallback expansion (mex can be behind enemy lines)
	const CMetalData::Metals& spots = metalManager->GetSpots();
	Map* map = circuit->GetMap();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CCircuitDef* mexDef = buildDef;
	circuit->GetThreatMap()->SetThreatType(unit);
	CMetalData::PointPredicate predicate = [&spots, economyManager, map, mexDef, terrainManager, unit](const int index) {
		return (economyManager->IsAllyOpenSpot(index) &&
				terrainManager->CanBeBuiltAtSafe(mexDef, spots[index].position) &&  // hostile environment
				terrainManager->CanBuildAtSafe(unit, spots[index].position) &&
				map->IsPossibleToBuildAt(mexDef->GetUnitDef(), spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
	};
	int index = metalManager->FindNearestSpot(position, predicate);

	if (index >= 0) {
		buildPos = spots[index].position;
		economyManager->SetOpenSpot(index, false);
		TRY_UNIT(circuit, unit,
			u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
		)
	} else {
//		buildPos = -RgtVector;
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

void CBMexTask::Cancel()
{
	if ((target == nullptr) && utils::is_valid(buildPos)) {
		CCircuitAI* circuit = manager->GetCircuit();
		int index = circuit->GetMetalManager()->FindNearestSpot(buildPos);
		circuit->GetMetalManager()->SetOpenSpot(index, true);
		circuit->GetEconomyManager()->SetOpenSpot(index, true);
	}
}

void CBMexTask::OnUnitIdle(CCircuitUnit* unit)
{
	/*
	 * Check if unit is idle because of enemy mex ahead and build turret if so.
	 */
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitDef* def = circuit->GetMilitaryManager()->GetDefaultPorc();
	if ((def == nullptr) || !def->IsAvailable(circuit->GetLastFrame())) {
		IBuilderTask::OnUnitIdle(unit);
		return;
	}

	const float range = def->GetMaxRange();
	const float testRange = range + 200.0f;  // 200 elmos
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	if (buildPos.SqDistance2D(pos) < SQUARE(testRange)) {
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
