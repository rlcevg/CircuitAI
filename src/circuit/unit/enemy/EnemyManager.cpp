/*
 * EnemyManager.cpp
 *
 *  Created on: Dec 25, 2019
 *      Author: rlcevg
 */

#include "unit/enemy/EnemyManager.h"
#include "unit/enemy/EnemyUnit.h"
#include "map/ThreatMap.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "json/json.h"

#include "WrappUnit.h"
#include "Cheats.h"

namespace circuit {

using namespace springai;

CEnemyManager::CEnemyManager(CCircuitAI* circuit)
		: circuit(circuit)
		, enemyIterator(0)
		, enemyMobileCost(0.f)
		, mobileThreat(0.f)
		, staticThreat(0.f)
{
	enemyPos = AIFloat3(circuit->GetTerrainManager()->GetTerrainWidth() / 2, 0, circuit->GetTerrainManager()->GetTerrainHeight() / 2);
	enemyGroups.push_back(SEnemyGroup(enemyPos));

	ReadConfig();
}

CEnemyManager::~CEnemyManager()
{
	for (auto& kv : enemyUnits) {
		delete kv.second;
	}
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
		UnitDef* unitDef = u->GetDef();
		if (unitDef == nullptr) {  // doesn't work with globalLOS
			delete u;
			return std::make_pair(nullptr, false);
		}
		cdef = circuit->GetCircuitDef(unitDef->GetUnitDefId());
		delete unitDef;
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
	UnitDef* unitDef = e->GetDef();
	CCircuitDef::Id unitDefId = unitDef->GetUnitDefId();
	delete unitDef;

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

void CEnemyManager::UpdateEnemyDatas()
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
	// -2 is for threat-draw frame and k-means frame
	unsigned int n = (enemyUpdates.size() / (THREAT_UPDATE_RATE - 2)) + 1;

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
		}

		++enemyIterator;
		--n;
	}

	if (!circuit->IsCheating()) {
		circuit->GetCheats()->SetEnabled(false);
	}
}

bool CEnemyManager::UnitInLOS(CEnemyUnit* data)
{
	UnitDef* unitDef = data->GetUnit()->GetDef();
	if (unitDef == nullptr) {  // doesn't work with globalLOS
		return false;
	}
	CCircuitDef::Id unitDefId = unitDef->GetUnitDefId();
	delete unitDef;
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

	for (CCircuitDef::RoleT i = 0; i < static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_); ++i) {
		if (cdef->IsEnemyRoleAny(CCircuitDef::GetMask(i))) {
			SEnemyInfo& info = enemyInfos[i];
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

	for (CCircuitDef::RoleT i = 0; i < static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_); ++i) {
		if (cdef->IsEnemyRoleAny(CCircuitDef::GetMask(i))) {
			SEnemyInfo& info = enemyInfos[i];
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
	const CCircuitAI::EnemyInfos& units = circuit->GetEnemyInfos();
	// calculate a new K. change the formula to adjust max K, needs to be 1 minimum.
	constexpr int KMEANS_BASE_MAX_K = 32;
	int newK = std::min(KMEANS_BASE_MAX_K, 1 + (int)sqrtf(units.size()));

	// change the number of means according to newK
	assert(newK > 0/* && enemyGoups.size() > 0*/);
	// add a new means, just use one of the positions
	AIFloat3 newMeansPosition = units.begin()->second->GetPos();
//	newMeansPosition.y = circuit->GetMap()->GetElevationAt(newMeansPosition.x, newMeansPosition.z) + K_MEANS_ELEVATION;
	enemyGroups.resize(newK, SEnemyGroup(newMeansPosition));

	// check all positions and assign them to means, complexity n*k for one iteration
	std::vector<int> unitsClosestMeanID(units.size(), -1);
	std::vector<int> numUnitsAssignedToMean(newK, 0);

	{
		int i = 0;
		for (const auto& kv : units) {
			CEnemyInfo* enemy = kv.second;
			if (enemy->GetData()->IsHidden()) {
				continue;
			}
			AIFloat3 unitPos = enemy->GetPos();
			float closestDistance = std::numeric_limits<float>::max();
			int closestIndex = -1;

			for (int m = 0; m < newK; m++) {
				const AIFloat3& mean = enemyGroups[m].pos;
				float distance = unitPos.SqDistance2D(mean);

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

	// change the means according to which positions are assigned to them
	// use meanAverage for indexes with 0 pos'es assigned
	// make a new means list
//	std::vector<AIFloat3> newMeans(newK, ZeroVector);
	std::vector<SEnemyGroup>& newMeans = enemyGroups;
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
		for (const auto& kv : units) {
			CEnemyInfo* enemy = kv.second;
			if (enemy->GetData()->IsHidden()) {
				continue;
			}
			int meanIndex = unitsClosestMeanID[i++];
			SEnemyGroup& eg = newMeans[meanIndex];

			// don't divide by 0
			float num = std::max(1, numUnitsAssignedToMean[meanIndex]);
			eg.pos += enemy->GetPos() / num;

			eg.units.push_back(kv.first);

			const CCircuitDef* cdef = enemy->GetCircuitDef();
			if (cdef != nullptr) {
				eg.roleCosts[cdef->GetMainRole()] += cdef->GetCost();
				if (!cdef->IsMobile() || enemy->GetData()->IsInRadarOrLOS()) {
					eg.cost += cdef->GetCost();
				}
				eg.threat += enemy->GetThreat() * (cdef->IsMobile() ? initThrMod.inMobile : initThrMod.inStatic);
			} else {
				eg.threat += enemy->GetThreat();
			}
		}
	}

	// do a check and see if there are any empty means and set the height
	enemyPos = ZeroVector;
	for (int i = 0; i < newK; i++) {
		// if a newmean is unchanged, set it to the new means pos instead of (0, 0, 0)
		if (newMeans[i].pos == ZeroVector) {
			newMeans[i] = newMeansPosition;
		} else {
			// get the proper elevation for the y-coord
//			newMeans[i].pos.y = circuit->GetMap()->GetElevationAt(newMeans[i].pos.x, newMeans[i].pos.z) + K_MEANS_ELEVATION;
		}
		enemyPos += newMeans[i].pos;
	}
	enemyPos /= newK;

//	return newMeans;
}

} // namespace circuit
