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
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBMexTask::CBMexTask(ITaskManager* mgr, Priority priority,
					 CCircuitDef* buildDef, const AIFloat3& position,
					 float cost, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, BuildType::MEX, cost, false, timeout)
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
		u->Build(target->GetCircuitDef()->GetUnitDef(), tu->GetPos(), tu->GetBuildingFacing(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
		return;
	}
	CCircuitAI* circuit = manager->GetCircuit();
	UnitDef* buildUDef = buildDef->GetUnitDef();
	if (buildPos != -RgtVector) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
			u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			return;
		} else {
			circuit->GetMetalManager()->SetOpenSpot(buildPos, true);
		}
	}

	buildPos = circuit->GetEconomyManager()->FindBuildPos(unit);
	if (buildPos != -RgtVector) {
		circuit->GetMetalManager()->SetOpenSpot(buildPos, false);
		u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

void CBMexTask::Finish()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CMetalManager* metalManager = circuit->GetMetalManager();

	int index = metalManager->FindNearestCluster(buildPos);
	if (index < 0) {
		circuit->GetEconomyManager()->UpdateMetalTasks(buildPos);
		return;
	}

	CBuilderManager* builderManager = circuit->GetBuilderManager();
	CEconomyManager* economyManager = circuit->GetEconomyManager();

	bool mustHave = !economyManager->IsEnergyStalling() && (builderManager->GetTasks(IBuilderTask::BuildType::MEX).size() < 1) && (builderManager->GetWorkerCount() > 1);
	if (mustHave && buildDef->IsAvailable()) {
		// Colonize next spot in cluster
		int bestIdx = -1;
		Map* map = circuit->GetMap();
		const CMetalData::Metals& spots = metalManager->GetSpots();
		float sqMinDist = std::numeric_limits<float>::max();
		for (auto idx : metalManager->GetClusters()[index].idxSpots) {
			const AIFloat3& pos = spots[idx].position;
			if (metalManager->IsOpenSpot(idx) &&
				builderManager->IsBuilderInArea(buildDef, pos) &&
				map->IsPossibleToBuildAt(buildDef->GetUnitDef(), pos, UNIT_COMMAND_BUILD_NO_FACING))
			{
				float sqDist = buildPos.SqDistance2D(pos);
				if (sqDist < sqMinDist) {
					sqMinDist = sqDist;
					bestIdx = idx;
				}
			}
		}
		if (bestIdx != -1) {
			const AIFloat3& pos = spots[bestIdx].position;
			metalManager->SetOpenSpot(bestIdx, false);
			builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, buildDef, pos, IBuilderTask::BuildType::MEX, cost)->SetBuildPos(pos);
		} else {
			circuit->GetEconomyManager()->UpdateMetalTasks(buildPos);
		}
	} else {
		economyManager->UpdateEnergyTasks(buildPos);
	}

	// Add defence
	// TODO: Move into MilitaryManager
	CCircuitDef* defDef;
	bool valid = false;
	float maxCost = MIN_BUILD_SEC * economyManager->GetAvgMetalIncome();/* * economyManager->GetEcoFactor();*/
	const char* defenders[] = {"corhlt", "corllt"};
	for (auto name : defenders) {
		defDef = circuit->GetCircuitDef(name);
		if (defDef->GetUnitDef()->GetCost(economyManager->GetMetalRes()) < maxCost) {
			valid = true;
			break;
		}
	}
	if (valid) {
		CMilitaryManager::SDefPoint* closestPoint = nullptr;
		float minDist = std::numeric_limits<float>::max();
		for (auto& defPoint : circuit->GetMilitaryManager()->GetDefPoints(index)) {
			if (defPoint.isOpen) {
				float dist = defPoint.position.SqDistance2D(buildPos);
				if ((closestPoint == nullptr) || (dist < minDist)) {
					closestPoint = &defPoint;
					minDist = dist;
				}
			}
		}
		if (closestPoint != nullptr) {
			closestPoint->isOpen = false;
			builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, defDef, closestPoint->position, IBuilderTask::BuildType::DEFENCE);
		}
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
	CCircuitDef* def = circuit->GetCircuitDef("corrl");
	float range = def->GetUnitDef()->GetMaxWeaponRange();
	float testRange = range + 200;
	const AIFloat3& pos = unit->GetUnit()->GetPos();
	if (buildPos.SqDistance2D(pos) < testRange * testRange) {
		int mexDefId = circuit->GetEconomyManager()->GetMexDef()->GetId();
		// TODO: Use internal CCircuitAI::GetEnemyUnits?
		auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(buildPos, 1));
		bool blocked = false;
		for (auto enemy : enemies) {
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
