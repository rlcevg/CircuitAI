/*
 * MilitaryManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/math/HierarchCluster.h"
#include "util/math/RagMatrix.h"
#include "util/math/EncloseCircle.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AISCommands.h"
// debug
#include "Pathing.h"
#include "OOAICallback.h"

namespace circuit {

using namespace springai;

CMilitaryManager::CMilitaryManager(CCircuitAI* circuit) :
		IUnitModule(circuit)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunParallelTask(CGameTask::emptyTask, std::make_shared<CGameTask>(&CMilitaryManager::Init, this));

	int unitDefId;

	auto atackerFinishedHandler = [this](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		CTerrainManager* terrain = this->circuit->GetTerrainManager();
		int terWidth = terrain->GetTerrainWidth();
		int terHeight = terrain->GetTerrainHeight();
		float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
		float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
		AIFloat3 fromPos(x, this->circuit->GetMap()->GetElevationAt(x, z), z);
		u->Fight(fromPos, UNIT_COMMAND_OPTION_SHIFT_KEY, FRAMES_PER_SEC * 60);
	};
	auto atackerIdleHandler = [this](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		CTerrainManager* terrain = this->circuit->GetTerrainManager();
		int terWidth = terrain->GetTerrainWidth();
		int terHeight = terrain->GetTerrainHeight();
		float x = rand() % (int)(terWidth + 1);
		float z = rand() % (int)(terHeight + 1);
		AIFloat3 toPos(x, this->circuit->GetMap()->GetElevationAt(x, z), z);
		u->Fight(toPos, 0, FRAMES_PER_SEC * 60 * 5);
	};

	unitDefId = circuit->GetCircuitDef("armpw")->GetId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
	unitDefId = circuit->GetCircuitDef("armrock")->GetId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
	unitDefId = circuit->GetCircuitDef("armwar")->GetId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
	unitDefId = circuit->GetCircuitDef("armzeus")->GetId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
	unitDefId = circuit->GetCircuitDef("armjeth")->GetId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
	unitDefId = circuit->GetCircuitDef("armsnipe")->GetId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;

	/*
	 * Defence handlers
	 */
	unitDefId = circuit->GetCircuitDef("corllt")->GetId();
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		OpenDefPoint(unit->GetUnit()->GetPos());
	};

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
//	damagedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
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
//	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
//		fighterInfos.erase(unit);
//	};
}

CMilitaryManager::~CMilitaryManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

int CMilitaryManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
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

//int CMilitaryManager::UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
//{
//	auto search = damagedHandler.find(unit->GetDef()->GetUnitDefId());
//	if (search != damagedHandler.end()) {
//		search->second(unit, attacker);
//	}
//
//	return 0; //signaling: OK
//}

int CMilitaryManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::EnemyEnterLOS(CCircuitUnit* unit)
{
	// debug
//	if (strcmp(unit->GetDef()->GetName(), "factorycloak") == 0) {
//		circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>([this](CCircuitUnit* unit) {
//			Unit* u = unit->GetUnit();
//			Pathing* pathing = circuit->GetPathing();
//			Map* map = circuit->GetMap();
//			const CMetalManager::Metals& spots = circuit->GetMetalManager().GetSpots();
//			const AIFloat3& start = u->GetPos();
//			for (auto& s : spots) {
//				AIFloat3 end = s.position;
//				int pathId = pathing->InitPath(start, end, 4, .0f);
//				AIFloat3 lastPoint, point(start);
//				Drawer* drawer = map->GetDrawer();
//				do {
//					lastPoint = point;
//					point = pathing->GetNextWaypoint(pathId);
//					drawer->AddLine(lastPoint, point);
//				} while (lastPoint != point);
//				delete drawer;
//				pathing->FreePath(pathId);
//			}
////			circuit->GetGame()->SetPause(true, "Nub");
//		}, unit), FRAMES_PER_SEC);
//	}

	return 0; //signaling: OK
}

void CMilitaryManager::AssignTask(CCircuitUnit* unit)
{

}

void CMilitaryManager::AbortTask(IUnitTask* task)
{

}

void CMilitaryManager::DoneTask(IUnitTask* task)
{

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

CMilitaryManager::DefPoints& CMilitaryManager::GetDefPoints(int index)
{
	return clusterInfos[index].defPoints;
}

void CMilitaryManager::OpenDefPoint(const AIFloat3& pos)
{
	int index = circuit->GetMetalManager()->FindNearestCluster(pos);
	if (index < 0) {
		return;
	}

	DefPoints& defPoints = clusterInfos[index].defPoints;
	int idx = 0;
	float dist = pos.distance2D(defPoints[idx].position);
	for (int i = 1; i < defPoints.size(); ++i) {
		if (!defPoints[i].isOpen) {
			float tmp = pos.distance2D(defPoints[i].position);
			if (tmp < dist) {
				tmp = dist;
				idx = i;
			}
		}
	}
	defPoints[idx].isOpen = true;
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
			defPoints.push_back({pos, true});
		}
	}
}

} // namespace circuit
