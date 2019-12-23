/*
 * MapManager.cpp
 *
 *  Created on: Dec 21, 2019
 *      Author: rlcevg
 */

#include "map/MapManager.h"
#include "map/ThreatMap.h"
#include "map/InfluenceMap.h"

#include "spring/SpringMap.h"

#include "OOAICallback.h"
#include "Mod.h"

namespace circuit {

using namespace springai;

CMapManager::CMapManager(CCircuitAI* circuit, float decloakRadius)
		: circuit(circuit)
{
	CMap* map = circuit->GetMap();
	int mapWidth = map->GetWidth();
	Mod* mod = circuit->GetCallback()->GetMod();
	int losMipLevel = mod->GetLosMipLevel();
	int radarMipLevel = mod->GetRadarMipLevel();
	delete mod;

//	radarMap = std::move(map->GetRadarMap());
	radarWidth = mapWidth >> radarMipLevel;
	map->GetSonarMap(sonarMap);
	radarResConv = SQUARE_SIZE << radarMipLevel;
	map->GetLosMap(losMap);
	losWidth = mapWidth >> losMipLevel;
	losResConv = SQUARE_SIZE << losMipLevel;

	threatMap = new CThreatMap(this, decloakRadius);
	inflMap = new CInfluenceMap(this);
}

CMapManager::~CMapManager()
{
	delete threatMap;
	delete inflMap;
}

void CMapManager::EnqueueUpdate()
{
	if (threatMap->IsUpdating() || inflMap->IsUpdating()) {
		return;
	}

//	radarMap = std::move(circuit->GetMap()->GetRadarMap());
	circuit->GetMap()->GetSonarMap(sonarMap);
	circuit->GetMap()->GetLosMap(losMap);

	hostileDatas.clear();
	hostileDatas.reserve(hostileUnits.size());
	for (auto& kv : hostileUnits) {
		CEnemyUnit* e = kv.second;
		if (e->IsHidden()) {
			continue;
		}

		if (e->NotInRadarAndLOS() && IsInLOS(e->GetPos())) {
			e->SetHidden();
			continue;
		}

		if (e->IsInLOS()) {
			threatMap->SetEnemyUnitThreat(e);
		}

		hostileDatas.push_back(e->GetData());
	}

	peaceDatas.clear();
	peaceDatas.reserve(peaceUnits.size());
	for (auto& kv : peaceUnits) {
		CEnemyUnit* e = kv.second;
		if (e->IsHidden()) {
			continue;
		}

		if (e->NotInRadarAndLOS() && IsInLOS(e->GetPos())) {
			e->SetHidden();
			continue;
		}

		peaceDatas.push_back(e->GetData());
	}

	threatMap->EnqueueUpdate();
	inflMap->EnqueueUpdate();
}

bool CMapManager::EnemyEnterLOS(CEnemyUnit* enemy)
{
	// Possible cases:
	// (1) Unknown enemy that has been detected for the first time
	// (2) Unknown enemy that was only in radar enters LOS
	// (3) Known enemy that already was in LOS enters again

	enemy->SetInLOS();
	const bool wasKnown = enemy->IsKnown();

	if (!enemy->IsAttacker()) {
		if (enemy->GetThreat() > .0f) {  // (2)
			// threat prediction failed when enemy was unknown
			if (enemy->IsHidden()) {
				enemy->ClearHidden();
			}
			hostileUnits.erase(enemy->GetId());
			peaceUnits[enemy->GetId()] = enemy;
			enemy->SetThreat(.0f);
			threatMap->SetEnemyUnitRange(enemy);
		} else if (peaceUnits.find(enemy->GetId()) == peaceUnits.end()) {
			peaceUnits[enemy->GetId()] = enemy;
			threatMap->SetEnemyUnitRange(enemy);
		} else if (enemy->IsHidden()) {
			enemy->ClearHidden();
		}

		enemy->UpdateInRadarData(enemy->GetUnit()->GetPos());
		enemy->UpdateInLosData();
		enemy->SetKnown();

		return !wasKnown;
	}

	if (hostileUnits.find(enemy->GetId()) == hostileUnits.end()) {
		hostileUnits[enemy->GetId()] = enemy;
	} else if (enemy->IsHidden()) {
		enemy->ClearHidden();
	}

	enemy->UpdateInRadarData(enemy->GetUnit()->GetPos());
	enemy->UpdateInLosData();
	threatMap->SetEnemyUnitRange(enemy);
	threatMap->SetEnemyUnitThreat(enemy);
	enemy->SetKnown();

	return !wasKnown;
}

void CMapManager::EnemyLeaveLOS(CEnemyUnit* enemy)
{
	enemy->ClearInLOS();
}

void CMapManager::EnemyEnterRadar(CEnemyUnit* enemy)
{
	// Possible cases:
	// (1) Unknown enemy wanders at radars
	// (2) Known enemy that once was in los wandering at radar
	// (3) EnemyEnterRadar invoked right after EnemyEnterLOS in area with no radar

	enemy->SetInRadar();

	if (enemy->IsInLOS()) {  // (3)
		return;
	}

	if (!enemy->IsAttacker()) {  // (2)
		if (enemy->IsHidden()) {
			enemy->ClearHidden();
		}

		enemy->UpdateInRadarData(enemy->GetUnit()->GetPos());

		return;
	}

	bool isNew = false;
	auto it = hostileUnits.find(enemy->GetId());
	if (it == hostileUnits.end()) {  // (1)
		std::tie(it, isNew) = hostileUnits.emplace(enemy->GetId(), enemy);
	} else if (enemy->IsHidden()) {
		enemy->ClearHidden();
	}

	enemy->UpdateInRadarData(enemy->GetUnit()->GetPos());
	if (isNew) {  // unknown enemy enters radar for the first time
		threatMap->NewEnemy(enemy);
	}
}

void CMapManager::EnemyLeaveRadar(CEnemyUnit* enemy)
{
	enemy->ClearInRadar();
}

bool CMapManager::EnemyDestroyed(CEnemyUnit* enemy)
{
	auto it = hostileUnits.find(enemy->GetId());
	if (it == hostileUnits.end()) {
		peaceUnits.erase(enemy->GetId());
		return enemy->IsKnown();
	}

	hostileUnits.erase(it);
	return enemy->IsKnown();
}

bool CMapManager::IsInLOS(const AIFloat3& pos) const
{
	// res = 1 << Mod->GetLosMipLevel();
	// the value for the full resolution position (x, z) is at index ((z * width + x) / res)
	// the last value, bottom right, is at index (width/res * height/res - 1)

	// FIXME: @see rts/Sim/Objects/SolidObject.cpp CSolidObject::UpdatePhysicalState
	//        for proper "underwater" implementation
	if (pos.y < -SQUARE_SIZE * 5) {  // Mod->GetRequireSonarUnderWater() = true
		const int x = (int)pos.x / radarResConv;
		const int z = (int)pos.z / radarResConv;
		if (sonarMap[z * radarWidth + x] <= 0) {
			return false;
		}
	}
	// convert from world coordinates to losmap coordinates
	const int x = (int)pos.x / losResConv;
	const int z = (int)pos.z / losResConv;
	return losMap[z * losWidth + x] > 0;
}

//bool CMapManager::IsInRadar(const AIFloat3& pos) const
//{
//	// the value for the full resolution position (x, z) is at index ((z * width + x) / res)
//	// the last value, bottom right, is at index (width/res * height/res - 1)
//
//	// convert from world coordinates to radarmap coordinates
//	const int x = (int)pos.x / radarResConv;
//	const int z = (int)pos.z / radarResConv;
//	return ((pos.y < -SQUARE_SIZE * 5) ? sonarMap : radarMap)[z * radarWidth + x] > 0;
//}

} // namespace circuit
