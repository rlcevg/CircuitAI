/*
 * AllyTeam.cpp
 *
 *  Created on: Apr 7, 2015
 *      Author: rlcevg
 */

#include "unit/ally/AllyTeam.h"
#include "unit/FactoryData.h"
#include "map/MapManager.h"
#include "map/ThreatMap.h"
#include "resource/MetalManager.h"
#include "resource/EnergyGrid.h"
#include "setup/DefenceMatrix.h"
#include "setup/SetupManager.h"
#include "terrain/PathFinder.h"
#include "terrain/TerrainData.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "spring/SpringCallback.h"

#include "AIFloat3.h"
#include "Team.h"

namespace circuit {

using namespace springai;

bool CAllyTeam::SBox::ContainsPoint(const AIFloat3& point) const
{
	return (point.x >= left) && (point.x <= right) &&
		   (point.z >= top) && (point.z <= bottom);
}

CAllyTeam::CAllyTeam(const TeamIds& tids, const SBox& sb)
		: circuit(nullptr)
		, teamIds(tids)
		, startBox(sb)
		, initCount(0)
		, resignSize(0)
		, lastUpdate(-1)
		, uEnemyMark(0)
{
}

CAllyTeam::~CAllyTeam()
{
	if (initCount > 0) {
		initCount = 1;
		Release();
	}
}

void CAllyTeam::Init(CCircuitAI* circuit, float decloakRadius)
{
	if (initCount++ > 0) {
		return;
	}

	this->circuit = circuit;

	int boxId = circuit->GetTeam()->GetRulesParamFloat("start_box_id", -1);
	if (boxId >= 0) {
		startBox = circuit->GetGameAttribute()->GetSetupData().GetStartBox(boxId);
	}

	mapManager = std::make_shared<CMapManager>(circuit, decloakRadius);
	enemyManager = std::make_shared<CEnemyManager>(circuit);

	uEnemyMark = circuit->GetSkirmishAIId() % THREAT_UPDATE_RATE;

	metalManager = std::make_shared<CMetalManager>(circuit, &circuit->GetGameAttribute()->GetMetalData());
	if (metalManager->HasMetalSpots() && !metalManager->HasMetalClusters() && !metalManager->IsClusterizing()) {
		metalManager->ClusterizeMetal(circuit->GetSetupManager()->GetCommChoice());
	}

	energyGrid = std::make_shared<CEnergyGrid>(circuit);
	defence = std::make_shared<CDefenceMatrix>(circuit);
	pathfinder = std::make_shared<CPathFinder>(&circuit->GetGameAttribute()->GetTerrainData());
	// FIXME: DEBUG
	pathfinder->ToggleVis(circuit);
	// FIXME: DEBUG
	factoryData = std::make_shared<CFactoryData>(circuit);

	circuit->GetScheduler()->RunOnRelease(std::make_shared<CGameTask>(&CAllyTeam::DelegateAuthority, this, circuit));
}

void CAllyTeam::Release()
{
	resignSize++;
	if (--initCount > 0) {
		return;
	}

	for (auto& kv : friendlyUnits) {
		delete kv.second;
	}
	friendlyUnits.clear();

	mapManager = nullptr;
	metalManager = nullptr;
	energyGrid = nullptr;
	defence = nullptr;
	pathfinder = nullptr;
	factoryData = nullptr;
}

void CAllyTeam::UpdateFriendlyUnits(CCircuitAI* circuit)
{
	if (lastUpdate >= circuit->GetLastFrame()) {
		return;
	}

	for (auto& kv : friendlyUnits) {
		delete kv.second;
	}
	friendlyUnits.clear();
	COOAICallback* clb = circuit->GetCallback();
	const std::vector<Unit*>& units = clb->GetFriendlyUnits();
	for (Unit* u : units) {
		if (u == nullptr) {  // engine returns vector with nullptrs
			continue;
		}
		int unitId = u->GetUnitId();
		CCircuitDef::Id unitDefId = clb->Unit_GetDefId(unitId);
		CAllyUnit* unit = new CAllyUnit(unitId, u, circuit->GetCircuitDef(unitDefId));
		friendlyUnits[unitId] = unit;
	}
	lastUpdate = circuit->GetLastFrame();
}

CAllyUnit* CAllyTeam::GetFriendlyUnit(ICoreUnit::Id unitId) const
{
	decltype(friendlyUnits)::const_iterator it = friendlyUnits.find(unitId);
	return (it != friendlyUnits.end()) ? it->second : nullptr;
}

bool CAllyTeam::EnemyInLOS(CEnemyUnit* data, CCircuitAI* ai)
{
	if (circuit != ai) {
		return true;
	}

	return enemyManager->UnitInLOS(data);
}

std::pair<CEnemyUnit*, bool> CAllyTeam::RegisterEnemyUnit(ICoreUnit::Id unitId, bool isInLOS, CCircuitAI* ai)
{
	if (circuit != ai) {
		return std::make_pair(enemyManager->GetEnemyUnit(unitId), true);
	}

	return enemyManager->RegisterEnemyUnit(unitId, isInLOS);
}

CEnemyUnit* CAllyTeam::RegisterEnemyUnit(Unit* e, CCircuitAI* ai)
{
	if (circuit != ai) {
		return enemyManager->GetEnemyUnit(e->GetUnitId());
	}

	return enemyManager->RegisterEnemyUnit(e);
}

void CAllyTeam::UnregisterEnemyUnit(CEnemyUnit* data, CCircuitAI* ai)
{
	if (circuit != ai) {
		return;
	}

	enemyManager->UnregisterEnemyUnit(data);
}

void CAllyTeam::EnemyEnterLOS(CEnemyUnit* enemy, CCircuitAI* ai)
{
	if (circuit != ai) {
		return;
	}

	if (mapManager->EnemyEnterLOS(enemy)) {
		enemyManager->AddEnemyCost(enemy);
	}
}

void CAllyTeam::EnemyLeaveLOS(CEnemyUnit* enemy, CCircuitAI* ai)
{
	if (circuit != ai) {
		return;
	}

	mapManager->EnemyLeaveLOS(enemy);
}

void CAllyTeam::EnemyEnterRadar(CEnemyUnit* enemy, CCircuitAI* ai)
{
	if (circuit != ai) {
		return;
	}

	mapManager->EnemyEnterRadar(enemy);
}

void CAllyTeam::EnemyLeaveRadar(CEnemyUnit* enemy, CCircuitAI* ai)
{
	if (circuit != ai) {
		return;
	}

	mapManager->EnemyLeaveRadar(enemy);
}

void CAllyTeam::EnemyDestroyed(CEnemyUnit* enemy, CCircuitAI* ai)
{
	if (circuit != ai) {
		return;
	}

	if (mapManager->EnemyDestroyed(enemy)) {
		enemyManager->DelEnemyCost(enemy);
	}
}

void CAllyTeam::Update(CCircuitAI* ai)
{
	if (circuit != ai) {
		return;
	}

//	if (!enemyInfos.empty()) {
		if (circuit->GetLastFrame() % THREAT_UPDATE_RATE == uEnemyMark) {
			EnqueueUpdate();
		} else {
			enemyManager->UpdateEnemyDatas();
		}
//	}
}

void CAllyTeam::EnqueueUpdate()
{
	if (enemyManager->IsUpdating() || mapManager->IsUpdating()) {
		return;
	}

	mapManager->PrepareUpdate();
	enemyManager->PrepareUpdate();

	mapManager->EnqueueUpdate();
	enemyManager->EnqueueUpdate();
}

void CAllyTeam::OccupyCluster(int clusterId, int teamId)
{
	auto it = occupants.find(clusterId);
	if (it != occupants.end()) {
		it->second.count++;
	} else {
		occupants.insert(std::make_pair(clusterId, SClusterTeam(teamId, 1)));
	}
}

CAllyTeam::SClusterTeam CAllyTeam::GetClusterTeam(int clusterId)
{
	auto it = occupants.find(clusterId);
	if (it != occupants.end()) {
		return it->second;
	}
	return SClusterTeam(-1);
}

// FIXME: DEBUG
//void CAllyTeam::OccupyArea(STerrainMapArea* area, int teamId)
//{
//	auto it = habitants.find(area);
//	if (it == habitants.end()) {
//		habitants.insert(std::make_pair(area, SAreaTeam(teamId)));
//	}
//}
//
//CAllyTeam::SAreaTeam CAllyTeam::GetAreaTeam(STerrainMapArea* area)
//{
//	auto it = habitants.find(area);
//	if (it != habitants.end()) {
//		return it->second;
//	}
//	return SAreaTeam(-1);
//}
// FIXME: DEBUG

void CAllyTeam::DelegateAuthority(CCircuitAI* curOwner)
{
	for (CCircuitAI* circuit : curOwner->GetGameAttribute()->GetCircuits()) {
		if (circuit->IsInitialized() && (circuit != curOwner) && (circuit->GetAllyTeamId() == curOwner->GetAllyTeamId())) {
			this->circuit = circuit;
			mapManager->SetAuthority(circuit);
			metalManager->SetAuthority(circuit);
			energyGrid->SetAuthority(circuit);
			circuit->GetScheduler()->RunOnRelease(std::make_shared<CGameTask>(&CAllyTeam::DelegateAuthority, this, circuit));
			break;
		}
	}
}

} // namespace circuit
