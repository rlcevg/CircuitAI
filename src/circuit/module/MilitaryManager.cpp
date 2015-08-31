/*
 * MilitaryManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "module/MilitaryManager.h"
#include "module/EconomyManager.h"
#include "resource/MetalManager.h"
#include "task/NullTask.h"
#include "task/IdleTask.h"
#include "task/RetreatTask.h"
#include "task/fighter/AttackTask.h"
#include "CircuitAI.h"
#include "util/math/HierarchCluster.h"
#include "util/math/RagMatrix.h"
#include "util/math/EncloseCircle.h"
#include "util/Scheduler.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CMilitaryManager::CMilitaryManager(CCircuitAI* circuit) :
		IUnitModule(circuit),
		updateSlice(0)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunParallelTask(CGameTask::emptyTask, std::make_shared<CGameTask>(&CMilitaryManager::Init, this));

	/*
	 * Attacker handlers
	 */
	auto attackerCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nullTask->AssignTo(unit);
		}
	};
	auto attackerFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			idleTask->AssignTo(unit);
		} else {
			nullTask->RemoveAssignee(unit);
		}
	};
	auto attackerIdleHandler = [this](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto attackerDamagedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		unit->GetTask()->OnUnitDamaged(unit, attacker);
	};
	auto attackerDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		unit->GetTask()->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask
	};

	CCircuitAI::CircuitDefs& defs = circuit->GetCircuitDefs();
	for (auto& kv : defs) {
		CCircuitDef* cdef = kv.second;
		UnitDef* def = cdef->GetUnitDef();
		if ((def->GetSpeed() > 0) && !def->IsBuilder() && (def->GetPower() > .0f)) {
			CCircuitDef::Id unitDefId = kv.first;
			createdHandler[unitDefId] = attackerCreatedHandler;
			finishedHandler[unitDefId] = attackerFinishedHandler;
			idleHandler[unitDefId] = attackerIdleHandler;
			damagedHandler[unitDefId] = attackerDamagedHandler;
			destroyedHandler[unitDefId] = attackerDestroyedHandler;
		}
	}

	CCircuitDef::Id unitDefId;
	/*
	 * Defence handlers
	 */
	auto defenceDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		Resource* metalRes = this->circuit->GetEconomyManager()->GetMetalRes();
		float defCost = unit->GetCircuitDef()->GetUnitDef()->GetCost(metalRes);
		SDefPoint* point = GetDefPoint(unit->GetUnit()->GetPos(), defCost);
		if (point != nullptr) {
			point->cost -= defCost;
		}
	};
	unitDefId = circuit->GetCircuitDef("corllt")->GetId();
	destroyedHandler[unitDefId] = defenceDestroyedHandler;
	unitDefId = circuit->GetCircuitDef("corhlt")->GetId();
	destroyedHandler[unitDefId] = defenceDestroyedHandler;

	/*
	 * raveparty handlers
	 */
	unitDefId = circuit->GetCircuitDef("raveparty")->GetId();
	finishedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		unit->GetUnit()->SetTrajectory(1);
	};

//	/*
//	 * armrectr handlers
//	 */
//	unitDefId = attrib->GetUnitDefByName("armrectr")->GetUnitDefId();
//	idleHandler[unitDefId] = [this](CCircuitUnit* unit) {
//		fighterInfos.erase(unit);
//	};
//	damagedHandler[unitDefId] = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
//		if (attacker != nullptr) {
//			auto search = fighterInfos.find(unit);
//			if (search == fighterInfos.end()) {
//				Unit* u = unit->GetUnit();
//				std::vector<float> params;
//				params.push_back(2.0f);
//				u->ExecuteCustomCommand(CMD_PRIORITY, params);
//
//				const AIFloat3& pos = attacker->GetUnit()->GetPos();
//				params.clear();
//				params.push_back(1.0f);  // 1: terraform_type, 1 == level
//				params.push_back(this->circuit->GetTeamId());  // 2: teamId
//				params.push_back(0.0f);  // 3: terraform type - 0 == Wall, else == Area
//				params.push_back(pos.y - 42.0f);  // 4: terraformHeight
//				params.push_back(1.0f);  // 5: number of control points
//				params.push_back(1.0f);  // 6: units count?
//				params.push_back(0.0f);  // 7: volumeSelection?
//				params.push_back(pos.x);  //  8: i + 0 control point x
//				params.push_back(pos.z);  //  9: i + 1 control point z
//				params.push_back(u->GetUnitId());  // 10: i + 2 unitId
//				u->ExecuteCustomCommand(CMD_TERRAFORM_INTERNAL, params);
//
//				fighterInfos[unit].isTerraforming = true;
//			}
//		}
//	};
//	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
//		fighterInfos.erase(unit);
//	};
}

CMilitaryManager::~CMilitaryManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(fighterTasks);
	utils::free_clear(deleteTasks);
}

int CMilitaryManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetCircuitDef()->GetId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitFinished(CCircuitUnit* unit)
{
	auto search = finishedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetCircuitDef()->GetId());
	if (search != idleHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	auto search = damagedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != damagedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

IUnitTask* CMilitaryManager::EnqueueTask()
{
	CAttackTask* task = new CAttackTask(this);
	fighterTasks.insert(task);
	return task;
}

void CMilitaryManager::DequeueTask(IUnitTask* task, bool done)
{
	auto it = fighterTasks.find(task);
	if (it != fighterTasks.end()) {
		fighterTasks.erase(it);
		task->Close(done);
		deleteTasks.insert(task);
	}
}

void CMilitaryManager::AssignTask(CCircuitUnit* unit)
{
	EnqueueTask()->AssignTo(unit);
}

void CMilitaryManager::AbortTask(IUnitTask* task)
{
	DequeueTask(task, false);
}

void CMilitaryManager::DoneTask(IUnitTask* task)
{
	DequeueTask(task, false);
}

void CMilitaryManager::SpecialCleanUp(CCircuitUnit* unit)
{

}

void CMilitaryManager::SpecialProcess(CCircuitUnit* unit)
{

}

void CMilitaryManager::FallbackTask(CCircuitUnit* unit)
{

}

CMilitaryManager::SDefPoint* CMilitaryManager::GetDefPoint(const AIFloat3& pos, float defCost)
{
	int index = circuit->GetMetalManager()->FindNearestCluster(pos);
	if (index < 0) {
		return nullptr;
	}

	DefPoints& defPoints = clusterInfos[index].defPoints;
	int idx = 0;
	float dist = pos.distance2D(defPoints[idx].position);
	for (int i = 1; i < defPoints.size(); ++i) {
		if (defPoints[i].cost >= defCost) {
			float tmp = pos.distance2D(defPoints[i].position);
			if (tmp < dist) {
				tmp = dist;
				idx = i;
			}
		}
	}
	return &defPoints[idx];
}

//const std::vector<CMilitaryManager::SClusterInfo>& CMilitaryManager::GetClusterInfos() const
//{
//	return clusterInfos;
//}

void CMilitaryManager::Init()
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Metals& spots = metalManager->GetSpots();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	clusterInfos.resize(clusters.size());

	Map* map = circuit->GetMap();
	float maxDistance = circuit->GetCircuitDef("corllt")->GetUnitDef()->GetMaxWeaponRange() * 3 / 4 * 2;
	CHierarchCluster clust;
	CEncloseCircle enclose;

	for (int k = 0; k < clusters.size(); ++k) {
		const CMetalData::MetalIndices& idxSpots = clusters[k].idxSpots;
		int nrows = idxSpots.size();
		CRagMatrix distmatrix(nrows);
		for (int i = 1; i < nrows; ++i) {
			for (int j = 0; j < i; ++j) {
				distmatrix(i, j) = spots[idxSpots[i]].position.distance2D(spots[idxSpots[j]].position);
			}
		}

		const CHierarchCluster::Clusters& iclusters = clust.Clusterize(distmatrix, maxDistance);

		DefPoints& defPoints = clusterInfos[k].defPoints;
		int nclusters = iclusters.size();
		defPoints.reserve(nclusters);
		for (int i = 0; i < nclusters; ++i) {
			std::vector<AIFloat3> points;
			points.reserve(iclusters[i].size());
			for (int j = 0; j < iclusters[i].size(); ++j) {
				points.push_back(spots[idxSpots[iclusters[i][j]]].position);
			}
			enclose.MakeCircle(points);
			AIFloat3 pos = enclose.GetCenter();
			pos.y = map->GetElevationAt(pos.x, pos.z);
			defPoints.push_back({pos, .0f});
		}
	}

	CScheduler* scheduler = circuit->GetScheduler().get();
	const int interval = 6;
	const int offset = circuit->GetSkirmishAIId() % interval + circuit->GetSkirmishAIId() * 2;
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::UpdateIdle, this), interval, offset + 3);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::UpdateRetreat, this), interval, offset + 4);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::UpdateFight, this), interval, offset + 5);
}

void CMilitaryManager::UpdateIdle()
{
	idleTask->Update();
}

void CMilitaryManager::UpdateRetreat()
{
	retreatTask->Update();
}

void CMilitaryManager::UpdateFight()
{
	if (!deleteTasks.empty()) {
		for (auto task : deleteTasks) {
			updateTasks.erase(task);
			delete task;
		}
		deleteTasks.clear();
	}

	auto it = updateTasks.begin();
	unsigned int i = 0;
	while (it != updateTasks.end()) {
		(*it)->Update();

		it = updateTasks.erase(it);
		if (++i >= updateSlice) {
			break;
		}
	}

	if (updateTasks.empty()) {
		updateTasks = fighterTasks;
		updateSlice = updateTasks.size() / TEAM_SLOWUPDATE_RATE;
	}
}

} // namespace circuit
