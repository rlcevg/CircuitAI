/*
 * EnemyManager.cpp
 *
 *  Created on: Dec 25, 2019
 *      Author: rlcevg
 */

#include "unit/enemy/EnemyManager.h"
#include "unit/enemy/EnemyUnit.h"
#include "map/MapManager.h"
#include "map/InfluenceMap.h"
#include "map/ThreatMap.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "json/json.h"

#include "spring/SpringCallback.h"

#include "WrappUnit.h"
#include "Cheats.h"

#include <limits>

namespace circuit {

using namespace springai;

CEnemyManager::CEnemyManager(CCircuitAI* circuit)
		: circuit(circuit)
		, enemyIterator(0)
		, pGroupData(&groupData0)
		, enemyGroups(groupData0.enemyGroups)
		, maxThreatGroupIdx(0)
		, isUpdating(false)
		, enemyMobileCost(0.f)
		, mobileThreat(0.f)
		, staticThreat(0.f)
		, isAreaUpdated(true)
{
	enemyPos = AIFloat3(circuit->GetTerrainManager()->GetTerrainWidth() / 2, 0, circuit->GetTerrainManager()->GetTerrainHeight() / 2);
	enemyGroups.push_back(SEnemyGroup(enemyPos));

	ReadConfig();
}

CEnemyManager::~CEnemyManager()
{
	for (CEnemyUnit* enemy : enemyUpdates) {
		if (enemyUnits.find(enemy->GetId()) == enemyUnits.end()) {
			delete enemy;
		}
	}
//	enemyUpdates.clear();
	for (auto& kv : enemyUnits) {
		delete kv.second;
	}
//	enemyUnits.clear();
}

CEnemyUnit* CEnemyManager::GetEnemyUnit(ICoreUnit::Id unitId) const
{
	auto it = enemyUnits.find(unitId);
	return (it != enemyUnits.end()) ? it->second : nullptr;
}

std::pair<CEnemyUnit*, bool> CEnemyManager::RegisterEnemyUnit(ICoreUnit::Id unitId, bool isInLOS)
{
	Unit* u = WrappUnit::GetInstance(circuit->GetSkirmishAIId(), unitId);
	if (u == nullptr) {
		return std::make_pair(nullptr, true);
	}
	if (/*u->IsNeutral() || */u->GetRulesParamFloat("ignoredByAI", 0.f) > 0.f) {
		delete u;
		return std::make_pair(nullptr, false);
	}
	CCircuitDef* cdef = nullptr;
	if (isInLOS) {
		CCircuitDef::Id unitDefId = circuit->GetCallback()->Unit_GetDefId(unitId);
		if (unitDefId == -1) {  // doesn't work with globalLOS
			delete u;
			return std::make_pair(nullptr, false);
		}
		cdef = circuit->GetCircuitDef(unitDefId);
	}
	CEnemyUnit* data = new CEnemyUnit(unitId, u, cdef);

	enemyUnits[data->GetId()] = data;
	enemyUpdates.push_back(data);

	return std::make_pair(data, true);
}

CEnemyUnit* CEnemyManager::RegisterEnemyUnit(Unit* e)
{
	if (/*e->IsNeutral() || */e->GetRulesParamFloat("ignoredByAI", 0.f) > 0.f) {
		return nullptr;
	}

	const ICoreUnit::Id unitId = e->GetUnitId();
	CEnemyUnit* data = GetEnemyUnit(unitId);
	CCircuitDef::Id unitDefId = circuit->GetCallback()->Unit_GetDefId(unitId);

	if (data != nullptr) {
		if ((data->GetCircuitDef() == nullptr) || data->GetCircuitDef()->GetId() != unitDefId) {
			data->SetCircuitDef(circuit->GetCircuitDef(unitDefId));
			data->SetCost(data->GetUnit()->GetRulesParamFloat("comm_cost", data->GetCost()));
		}
		return nullptr;
	}

	CCircuitDef* cdef = circuit->GetCircuitDef(unitDefId);
	data = new CEnemyUnit(unitId, e, cdef);

	enemyUnits[data->GetId()] = data;
	enemyUpdates.push_back(data);

	return data;
}

void CEnemyManager::UpdateEnemyDatas(CQuadField& quadField)
{
	if (!circuit->IsCheating()) {
		// AI knows what units are in los, hence reduce the amount of useless
		// engine InLos checks for each single param of the enemy unit
		circuit->GetCheats()->SetEnabled(true);
	}

	if (enemyIterator >= enemyUpdates.size()) {
		enemyIterator = 0;
	}
	enemyGarbage.clear();

	// stagger the Update's
	// -1 is for threat-draw and k-means frame
	unsigned int n = (enemyUpdates.size() / (THREAT_UPDATE_RATE - 1)) + 1;

	const int maxFrame = circuit->GetLastFrame() - FRAMES_PER_SEC * 60 * 20;
	while ((enemyIterator < enemyUpdates.size()) && (n != 0)) {
		CEnemyUnit* enemy = enemyUpdates[enemyIterator];
		if (enemy->IsDead()) {
			DeleteEnemyUnit(enemy);
			continue;
		}

		int frame = enemy->GetLastSeen();
		if ((frame != -1) && (maxFrame >= frame)) {
			GarbageEnemy(enemy);
			continue;
		}

		if (enemy->IsInRadarOrLOS()) {
			const AIFloat3& pos = enemy->GetUnit()->GetPos();
			if (CTerrainData::IsNotInBounds(pos)) {  // NOTE: Unit id validation. No EnemyDestroyed sometimes apparently
				GarbageEnemy(enemy);
				continue;
			}
			enemy->UpdateInRadarData(pos);
			if (enemy->IsInLOS()) {
				enemy->UpdateInLosData();  // heavy on engine calls
			}

			quadField.MovedEnemyUnit(enemy);
		}

		++enemyIterator;
		--n;
	}

	if (!circuit->IsCheating()) {
		circuit->GetCheats()->SetEnabled(false);
	}
}

void CEnemyManager::PrepareUpdate()
{
	CMapManager* mapManager = circuit->GetMapManager();

	hostileDatas.clear();
	hostileDatas.reserve(mapManager->GetHostileUnits().size());
	for (auto& kv : mapManager->GetHostileUnits()) {
		CEnemyUnit* e = kv.second;

		if (!mapManager->HostileInLOS(e)) {
			continue;
		}

		hostileDatas.push_back(e->GetData());
	}

	peaceDatas.clear();
	peaceDatas.reserve(mapManager->GetPeaceUnits().size());
	for (auto& kv : mapManager->GetPeaceUnits()) {
		CEnemyUnit* e = kv.second;

		if (!mapManager->PeaceInLOS(e)) {
			continue;
		}

		peaceDatas.push_back(e->GetData());
	}
}

void CEnemyManager::EnqueueUpdate()
{
//	if (isUpdating) {
//		return;
//	}
	isUpdating = true;

	circuit->GetScheduler()->RunParallelTask(std::make_shared<CGameTask>(&CEnemyManager::Update, this),
											 std::make_shared<CGameTask>(&CEnemyManager::Apply, this));
}

bool CEnemyManager::UnitInLOS(CEnemyUnit* data)
{
	CCircuitDef::Id unitDefId = circuit->GetCallback()->Unit_GetDefId(data->GetId());
	if (unitDefId == -1) {  // doesn't work with globalLOS
		return false;
	}
	if ((data->GetCircuitDef() == nullptr) || data->GetCircuitDef()->GetId() != unitDefId) {
		data->SetCircuitDef(circuit->GetCircuitDef(unitDefId));
		data->SetCost(data->GetUnit()->GetRulesParamFloat("comm_cost", data->GetCost()));
	}
	return true;
}

void CEnemyManager::UnregisterEnemyUnit(CEnemyUnit* data)
{
	enemyUnits.erase(data->GetId());
	data->Dead();
}

void CEnemyManager::DeleteEnemyUnit(CEnemyUnit* data)
{
	enemyUpdates[enemyIterator] = enemyUpdates.back();
	enemyUpdates.pop_back();

	delete data;
}

void CEnemyManager::GarbageEnemy(CEnemyUnit* enemy)
{
	enemyGarbage.push_back(enemy->GetId());
	UnregisterEnemyUnit(enemy);
	++enemyIterator;
}

void CEnemyManager::AddEnemyCost(const CEnemyUnit* e)
{
	CCircuitDef* cdef = e->GetCircuitDef();
	assert(cdef != nullptr);

	const CCircuitDef::RoleT roleSize = CCircuitDef::GetRoleNames().size();
	for (CCircuitDef::RoleT type = 0; type < roleSize; ++type) {
		if (cdef->IsEnemyRoleAny(CCircuitDef::GetMask(type))) {
			SEnemyInfo& info = enemyInfos[type];
			info.cost   += e->GetCost();
			info.threat += cdef->GetThreat();
		}
	}
	if (cdef->IsMobile()) {
		mobileThreat += cdef->GetThreat() * initThrMod.inMobile;
		enemyMobileCost += e->GetCost();
	} else {
		staticThreat += cdef->GetThreat() * initThrMod.inStatic;
	}
}

void CEnemyManager::DelEnemyCost(const CEnemyUnit* e)
{
	CCircuitDef* cdef = e->GetCircuitDef();
	assert(cdef != nullptr);

	const CCircuitDef::RoleT roleSize = CCircuitDef::GetRoleNames().size();
	for (CCircuitDef::RoleT type = 0; type < roleSize; ++type) {
		if (cdef->IsEnemyRoleAny(CCircuitDef::GetMask(type))) {
			SEnemyInfo& info = enemyInfos[type];
			info.cost   = std::max(info.cost   - e->GetCost(),      0.f);
			info.threat = std::max(info.threat - cdef->GetThreat(), 0.f);
		}
	}
	if (cdef->IsMobile()) {
		mobileThreat = std::max(mobileThreat - cdef->GetThreat() * initThrMod.inMobile, 0.f);
		enemyMobileCost = std::max(enemyMobileCost - e->GetCost(), 0.f);
	} else {
		staticThreat = std::max(staticThreat - cdef->GetThreat() * initThrMod.inStatic, 0.f);
	}
}

bool CEnemyManager::IsEnemyNear(const AIFloat3& pos, float maxThreat)
{
	CMilitaryManager* militaryMgr = circuit->GetMilitaryManager();
	CInfluenceMap* inflMap = circuit->GetInflMap();
	const AIFloat3& basePos = circuit->GetSetupManager()->GetBasePos();
	const float sqBaseRad = SQUARE(militaryMgr->GetBaseDefRange());
	const float sqCommRad = SQUARE(militaryMgr->GetCommDefRad(basePos.distance2D(pos)));
	for (const CEnemyManager::SEnemyGroup& group : enemyGroups) {
		if ((group.threat > 0.01f) && (group.threat < maxThreat)
			&& (basePos.SqDistance2D(group.pos) < sqBaseRad)
			&& (pos.SqDistance2D(group.pos) < sqCommRad)
			&& (inflMap->GetAllyDefendInflAt(group.pos) > INFL_EPS))
		{
			return true;
		}
	}
	return false;
}

void CEnemyManager::UpdateAreaUsers()
{
	if (isAreaUpdated) {
		return;
	}
	isAreaUpdated = true;

	areaInfo.clear();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	const std::vector<STerrainMapMobileType>& mobileTypes = terrainMgr->GetMobileTypes();
	for (const CEnemyManager::SEnemyGroup& group : enemyGroups) {
		const int iS = terrainMgr->GetSectorIndex(group.pos);
		for (const STerrainMapMobileType& mt : mobileTypes) {
			STerrainMapArea* area = mt.sector[iS].area;
			if (area != nullptr) {
				areaInfo.insert(area);
			}
		}
	}
}

void CEnemyManager::ReadConfig()
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const Json::Value& quotas = root["quota"];
	const Json::Value& qthrMod = quotas["thr_mod"];

	initThrMod.inMobile = qthrMod.get("mobile", 1.f).asFloat();
	initThrMod.inStatic = qthrMod.get("static", 0.f).asFloat();
	maxAAThreat = quotas.get("aa_threat", 42.f).asFloat();
}

/*
 * 2d only, ignores y component.
 * @see KAIK/AttackHandler::KMeansIteration for general reference
 */
void CEnemyManager::KMeansIteration()
{
	SGroupData& groupData = *GetNextGroupData();

	// calculate a new K. change the formula to adjust max K, needs to be 1 minimum.
	constexpr int KMEANS_BASE_MAX_K = 32;
	const auto enemySize = hostileDatas.size() + peaceDatas.size();
	int newK = std::min(KMEANS_BASE_MAX_K, 1 + (int)sqrtf(enemySize));

	// change the number of means according to newK
	assert(newK > 0/* && enemyGoups.size() > 0*/);
	// add a new means, just use one of the positions
	AIFloat3 newMeansPosition = hostileDatas.empty()
			? (peaceDatas.empty() ? enemyPos : peaceDatas.begin()->pos)
			: hostileDatas.begin()->pos;
//	newMeansPosition.y = circuit->GetMap()->GetElevationAt(newMeansPosition.x, newMeansPosition.z) + K_MEANS_ELEVATION;
	groupData.enemyGroups.resize(newK, SEnemyGroup(newMeansPosition));

	// check all positions and assign them to means, complexity n*k for one iteration
	std::vector<int> unitsClosestMeanID(enemySize, -1);
	std::vector<int> numUnitsAssignedToMean(newK, 0);

	{
		int i = 0;
		for (const std::vector<SEnemyData>& datas : {hostileDatas, peaceDatas}) {
			for (const SEnemyData& enemy : datas) {
				float closestDistance = std::numeric_limits<float>::max();
				int closestIndex = -1;

				for (int m = 0; m < newK; m++) {
					const AIFloat3& mean = groupData.enemyGroups[m].pos;
					float distance = enemy.pos.SqDistance2D(mean);

					if (distance < closestDistance) {
						closestDistance = distance;
						closestIndex = m;
					}
				}

				// position i is closest to the mean at closestIndex
				unitsClosestMeanID[i++] = closestIndex;
				numUnitsAssignedToMean[closestIndex]++;
			}
		}
	}

	// change the means according to which positions are assigned to them
	// use meanAverage for indexes with 0 pos'es assigned
	// make a new means list
//	std::vector<AIFloat3> newMeans(newK, ZeroVector);
	std::vector<SEnemyGroup>& newMeans = groupData.enemyGroups;
	for (unsigned i = 0; i < newMeans.size(); i++) {
		SEnemyGroup& eg = newMeans[i];
		eg.units.clear();
		eg.units.reserve(numUnitsAssignedToMean[i]);
		eg.pos = ZeroVector;
		std::fill(eg.roleCosts.begin(), eg.roleCosts.end(), 0.f);
		eg.cost = 0.f;
		eg.threat = 0.f;
	}

	{
		int i = 0;
		for (const std::vector<SEnemyData>& datas : {hostileDatas, peaceDatas}) {
			for (const SEnemyData& enemy : datas) {
				int meanIndex = unitsClosestMeanID[i++];
				SEnemyGroup& eg = newMeans[meanIndex];

				// don't divide by 0
				float num = std::max(1, numUnitsAssignedToMean[meanIndex]);
				eg.pos += enemy.pos / num;

				eg.units.push_back(enemy.id);

				if (enemy.cdef != nullptr) {
					eg.roleCosts[enemy.cdef->GetMainRole()] += enemy.cost;
					if (!enemy.cdef->IsMobile() || enemy.IsInRadarOrLOS()) {
						eg.cost += enemy.cost;
					}
					eg.threat += enemy.threat * (enemy.cdef->IsMobile() ? initThrMod.inMobile : initThrMod.inStatic);
				} else {
					eg.threat += enemy.threat;
				}
			}
		}
	}

	// do a check and see if there are any empty means and set the height
	groupData.enemyPos = ZeroVector;
	groupData.maxThreatGroupIdx = 0;
	for (int i = 0; i < newK; i++) {
		// if a newmean is unchanged, set it to the new means pos instead of (0, 0, 0)
		if (newMeans[i].pos == ZeroVector) {
			newMeans[i] = newMeansPosition;
		} else {
			// get the proper elevation for the y-coord
//			newMeans[i].pos.y = circuit->GetMap()->GetElevationAt(newMeans[i].pos.x, newMeans[i].pos.z) + K_MEANS_ELEVATION;
		}
		groupData.enemyPos += newMeans[i].pos;
		if (newMeans[groupData.maxThreatGroupIdx].threat < newMeans[i].threat) {
			groupData.maxThreatGroupIdx = i;
		}
	}
	groupData.enemyPos /= newK;

//	return newMeans;
}

void CEnemyManager::Prepare(SGroupData& groupData)
{
	groupData.enemyGroups = pGroupData.load()->enemyGroups;
}

void CEnemyManager::Update()
{
	Prepare(*GetNextGroupData());

	KMeansIteration();
}

void CEnemyManager::Apply()
{
	SwapBuffers();
	isUpdating = false;
}

void CEnemyManager::SwapBuffers()
{
	pGroupData = GetNextGroupData();
	SGroupData& groupData = *pGroupData.load();
	enemyGroups.swap(groupData.enemyGroups);
	enemyPos = groupData.enemyPos;
	maxThreatGroupIdx = groupData.maxThreatGroupIdx;
}

} // namespace circuit
