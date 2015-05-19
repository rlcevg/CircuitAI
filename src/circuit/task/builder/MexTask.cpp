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
	CMetalManager* metalManager = circuit->GetMetalManager();
	UnitDef* buildUDef = buildDef->GetUnitDef();
	if (buildPos != -RgtVector) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
			u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			return;
		} else {
			metalManager->SetOpenSpot(buildPos, true);
		}
	}

	const CMetalData::Metals& spots = metalManager->GetSpots();
	Map* map = circuit->GetMap();
	CTerrainManager* terrain = circuit->GetTerrainManager();
	CMetalData::MetalPredicate predicate = [&spots, metalManager, map, buildUDef, terrain, unit](CMetalData::MetalNode const& v) {
		int index = v.second;
		return (metalManager->IsOpenSpot(index) &&
				terrain->CanBuildAt(unit, spots[index].position) &&
				map->IsPossibleToBuildAt(buildUDef, spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
	};
	int index = metalManager->FindNearestSpot(position, predicate);
	buildPos = (index >= 0) ? spots[index].position : AIFloat3(-RgtVector);

	if (buildPos != -RgtVector) {
		metalManager->SetOpenSpot(buildPos, false);
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
		circuit->GetEconomyManager()->UpdateMetalTasks(buildPos, units.empty() ? nullptr : *units.begin());
		return;
	}

	CBuilderManager* builderManager = circuit->GetBuilderManager();
	CEconomyManager* economyManager = circuit->GetEconomyManager();

	int taskSize = builderManager->GetTasks(IBuilderTask::BuildType::MEX).size();
	bool mustHave = !economyManager->IsEnergyStalling() || ((taskSize < 1) && (builderManager->GetWorkerCount() > 2));
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
			IBuilderTask* task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, buildDef, pos, IBuilderTask::BuildType::MEX, cost);
			task->SetBuildPos(pos);
			metalManager->SetOpenSpot(bestIdx, false);
		} else {
			circuit->GetEconomyManager()->UpdateMetalTasks(buildPos, units.empty() ? nullptr : *units.begin());
		}
	} else {
		economyManager->UpdateEnergyTasks(buildPos, units.empty() ? nullptr : *units.begin());
	}

	// Add defence
	// TODO: Move into MilitaryManager
	CCircuitDef* defDef;
	bool valid = false;
	float maxCost = MIN_BUILD_SEC * std::min(economyManager->GetAvgMetalIncome(), economyManager->GetAvgEnergyIncome());
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
	if ((target == nullptr) && (buildPos != -RgtVector)) {
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
	float testRange = range + 200;  // 200 elmos
	const AIFloat3& pos = unit->GetUnit()->GetPos();
	if (buildPos.SqDistance2D(pos) < testRange * testRange) {
		int mexDefId = circuit->GetEconomyManager()->GetMexDef()->GetId();
		// TODO: Use internal CCircuitAI::GetEnemyUnits?
		auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(buildPos, SQUARE_SIZE));
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
			float qdist = 200 * 200;  // 200 elmos
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
