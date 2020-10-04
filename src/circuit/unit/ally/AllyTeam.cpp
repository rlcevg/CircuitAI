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
#include "terrain/path/PathFinder.h"
#include "terrain/TerrainData.h"
#include "CircuitAI.h"
#include "util/math/RayBox.h"
#include "util/GameAttribute.h"
#include "util/Scheduler.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

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

const CAllyTeam::Id CAllyTeam::GetLeaderId() const
{
	return circuit->GetTeamId();
}

void CAllyTeam::Init(CCircuitAI* circuit, float decloakRadius)
{
	if (initCount++ > 0) {
		return;
	}

	this->circuit = circuit;

	quadField.Init(int2(circuit->GetMap()->GetWidth(), circuit->GetMap()->GetHeight()), CQuadField::BASE_QUAD_SIZE);

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
	pathfinder = std::make_shared<CPathFinder>(circuit->GetScheduler(), &circuit->GetGameAttribute()->GetTerrainData());
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

	quadField.Kill();
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
		return !data->IsIgnore();
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
		const ICoreUnit::Id unitId = e->GetUnitId();
		delete e;
		return enemyManager->GetEnemyUnit(unitId);
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

void CAllyTeam::RegisterEnemyFake(CCircuitDef* cdef, const AIFloat3& pos, int timeout)
{
	CEnemyFake* data = enemyManager->RegisterEnemyFake(cdef, pos, timeout);
	mapManager->AddFakeEnemy(data);
	quadField.MovedEnemyFake(data);
}

void CAllyTeam::UnregisterEnemyFake(CEnemyFake* data)
{
	quadField.RemoveEnemyFake(data);
	mapManager->DelFakeEnemy(data);
	enemyManager->UnregisterEnemyFake(data);
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

	quadField.RemoveEnemyUnit(enemy);
}

void CAllyTeam::Update(CCircuitAI* ai)
{
	if (circuit != ai) {
		return;
	}

	if (circuit->GetLastFrame() % THREAT_UPDATE_RATE == uEnemyMark) {
		EnqueueUpdate();
	} else {
		enemyManager->UpdateEnemyDatas(quadField);
	}
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

bool CAllyTeam::IsEnemyOrFakeIn(const AIFloat3& startPos, const AIFloat3& dir, float length,
		const AIFloat3& enemyPos, float radius, const std::set<CCircuitDef::Id>& unitDefIds)
{
	QuadFieldQuery qfQuery(quadField);
	quadField.GetEnemyAndFakes(qfQuery, enemyPos, radius);  // TODO: predicate
	for (CEnemyUnit* e : *qfQuery.enemyUnits) {
		CCircuitDef* edef = e->GetCircuitDef();
		if ((edef != nullptr) && (unitDefIds.find(edef->GetId()) != unitDefIds.end())) {
			return true;
		}
	}
	for (CEnemyUnit* e : *qfQuery.enemyFakes) {
		CCircuitDef* edef = e->GetCircuitDef();
		if (unitDefIds.find(edef->GetId()) != unitDefIds.end()) {
			return true;
		}
	}

	const CRay ray(startPos, dir);
	const AIFloat3 offset(SQUARE_SIZE * 8, SQUARE_SIZE * 8, SQUARE_SIZE * 8);
	// WARNING: Do not reuse QuadFieldQuery within same scope. Otherwise cached vector may stuck in limbo.
	//   In this particular case it looks safe because quadField.GetEnemyAndFakes reserves enemyUnits and enemyFakes,
	//   while quadField.GetQuadsOnRay reserves quads only.
	quadField.GetQuadsOnRay(qfQuery, startPos, dir, length - radius);
	for (const int quadIdx: *qfQuery.quads) {
		const CQuadField::Quad& quad = quadField.GetQuad(quadIdx);

		for (CEnemyUnit* e : quad.enemyUnits) {
			CCircuitDef* edef = e->GetCircuitDef();
			if ((edef != nullptr) && (unitDefIds.find(edef->GetId()) != unitDefIds.end())
				&& CAABBox(e->GetPos() - offset, e->GetPos() + offset).Intersection(ray))
			{
				return true;
			}
		}
		for (CEnemyFake* e : quad.enemyFakes) {
			CCircuitDef* edef = e->GetCircuitDef();
			if (unitDefIds.find(edef->GetId()) != unitDefIds.end()
				&& CAABBox(e->GetPos() - offset, e->GetPos() + offset).Intersection(ray))
			{
				return true;
			}
		}
	}

	return false;
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
