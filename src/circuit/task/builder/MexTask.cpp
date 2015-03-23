/*
 * MexTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/MexTask.h"
#include "task/TaskManager.h"
#include "unit/CircuitUnit.h"
#include "module/EconomyManager.h"
#include "module/BuilderManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "UnitDef.h"
#include "Unit.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBMexTask::CBMexTask(ITaskManager* mgr, Priority priority,
					 UnitDef* buildDef, const AIFloat3& position,
					 float cost, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, BuildType::MEX, cost, timeout)
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
	CCircuitAI* circuit = manager->GetCircuit();
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
		manager->FallbackTask(unit);
	}
}

void CBMexTask::Finish()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CMetalManager* metalManager = circuit->GetMetalManager();
	IBuilderTask* task = nullptr;

	int index = metalManager->FindNearestCluster(buildPos);
	if (index >= 0) {
		CBuilderManager* builderManager = circuit->GetBuilderManager();

		// Colonize next spot in cluster
		Map* map = circuit->GetMap();
		const CMetalData::Metals& spots = metalManager->GetSpots();
		for (auto idx : metalManager->GetClusters()[index].idxSpots) {
			const AIFloat3& pos = spots[idx].position;
			if (metalManager->IsOpenSpot(idx) &&
				builderManager->IsBuilderInArea(buildDef, pos) &&
				map->IsPossibleToBuildAt(buildDef, pos, UNIT_COMMAND_BUILD_NO_FACING))
			{
				metalManager->SetOpenSpot(idx, false);
				task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, buildDef, pos, IBuilderTask::BuildType::MEX, cost);
				task->SetBuildPos(pos);
				break;
			}
		}

		// Add defence
		UnitDef* defDef = circuit->GetUnitDefByName("corllt");
		CEconomyManager* economyManager = circuit->GetEconomyManager();
		if (defDef->GetCost(economyManager->GetMetalRes()) / economyManager->GetAvgMetalIncome() < MIN_BUILD_SEC / 2) {
			for (auto& defPoint : circuit->GetMilitaryManager()->GetDefPoints(index)) {
				if (defPoint.isOpen) {
					defPoint.isOpen = false;
					builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, defDef, defPoint.position,
												IBuilderTask::BuildType::DEFENCE);
					break;
				}
			}
		}
	}

	if (task == nullptr) {
		circuit->GetEconomyManager()->UpdateMetalTasks(buildPos);
	}
}

void CBMexTask::Cancel()
{
	if (target == nullptr) {
		manager->GetCircuit()->GetMetalManager()->SetOpenSpot(buildPos, true);
	}
}

void CBMexTask::OnUnitIdle(CCircuitUnit* unit)
{
	/*
	 * Check if unit is idle because of enemy mex ahead and build turret if so.
	 */
	CCircuitAI* circuit = manager->GetCircuit();
	UnitDef* def = circuit->GetUnitDefByName("corrl");
	float range = def->GetMaxWeaponRange();
	float testRange = range + 200;
	const AIFloat3& pos = unit->GetUnit()->GetPos();
	if (buildPos.SqDistance2D(pos) < testRange * testRange) {
		int mexDefId = circuit->GetEconomyManager()->GetMexDef()->GetUnitDefId();
		// TODO: Use internal CCircuitAI::GetEnemyUnits?
		std::vector<Unit*> enemies = circuit->GetCallback()->GetEnemyUnitsIn(buildPos, 1);
		bool blocked = false;
		for (auto enemy : enemies) {
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
			float qdist = 200 * 200;
			// TODO: Push tasks into bgi::rtree
			for (auto t : builderManager->GetTasks(IBuilderTask::BuildType::DEFENCE)) {
				if (pos.SqDistance2D(t->GetTaskPos()) < qdist) {
					task = t;
					break;
				}
			}
			if (task == nullptr) {
				AIFloat3 newPos = buildPos - (buildPos - pos).Normalize2D() * range * 0.9;
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
