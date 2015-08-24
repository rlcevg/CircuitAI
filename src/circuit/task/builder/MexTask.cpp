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
	// Add defence
	// TODO: Move into MilitaryManager
	int index = circuit->GetMetalManager()->FindNearestCluster(buildPos);
	if (index < 0) {
		return;
	}
	CCircuitDef* defDef;
	CMilitaryManager* militaryManager = circuit->GetMilitaryManager();
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	float maxCost = MIN_BUILD_SEC * std::min(economyManager->GetAvgMetalIncome(), economyManager->GetAvgEnergyIncome());
	CMilitaryManager::SDefPoint* closestPoint = nullptr;
	float minDist = std::numeric_limits<float>::max();
	for (CMilitaryManager::SDefPoint& defPoint : militaryManager->GetDefPoints(index)) {
		if (defPoint.cost < maxCost) {
			float dist = defPoint.position.SqDistance2D(buildPos);
			if ((closestPoint == nullptr) || (dist < minDist)) {
				closestPoint = &defPoint;
				minDist = dist;
			}
		}
	}
	if (closestPoint == nullptr) {
		return;
	}
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	Resource* metalRes = economyManager->GetMetalRes();
	float totalCost = .0f;
	IBuilderTask* parentTask = nullptr;
	const char* defenders[] = {"corllt", "corhlt", "corrazor", "armnanotc", "cordoom"/*, "armartic", "corjamt", "armanni", "corbhmth"*/};
	for (const char* name : defenders) {
		defDef = circuit->GetCircuitDef(name);
		float defCost = defDef->GetUnitDef()->GetCost(metalRes);
		if (totalCost < closestPoint->cost) {
			totalCost += defCost;
			continue;
		}
		totalCost += defCost;
		if (totalCost < maxCost) {
			closestPoint->cost += defCost;
			IBuilderTask* task = builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, defDef, closestPoint->position,
															 IBuilderTask::BuildType::DEFENCE, true, (parentTask == nullptr));
			if (parentTask != nullptr) {
				parentTask->SetNextTask(task);
			}
			parentTask = task;
		} else {
			// TODO: Auto-sort defenders by cost OR remove break?
			break;
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
	float testRange = range + 200.0f;  // 200 elmos
	const AIFloat3& pos = unit->GetUnit()->GetPos();
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
			float qdist = 200.0f * 200.0f;  // 200 elmos
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
