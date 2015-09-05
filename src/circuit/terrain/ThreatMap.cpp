/*
 * ThreatMap.cpp
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 *      Original implementation: https://github.com/spring/KAIK/blob/master/ThreatMap.cpp
 */

#include "terrain/ThreatMap.h"
#include "terrain/TerrainManager.h"
#include "unit/EnemyUnit.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "Mod.h"

namespace circuit {

using namespace springai;

#define THREAT_RES			8
#define THREAT_VAL_BASE		1.0f

CThreatMap::CThreatMap(CCircuitAI* circuit)
		: circuit(circuit)
		, currMaxThreat(.0f)  // maximum threat (normalizer)
		, currSumThreat(.0f)  // threat summed over all cells
		, currAvgThreat(.0f)  // average threat over all cells
#ifdef DEBUG_VIS
		, sdlWindowId(DEBUG_MARK)
		, dbgMap(nullptr)
#endif
{
	width = circuit->GetTerrainManager()->GetTerrainWidth() / (SQUARE_SIZE * THREAT_RES);
	height = circuit->GetTerrainManager()->GetTerrainHeight() / (SQUARE_SIZE * THREAT_RES);
	threatCells.resize(width * height, THREAT_VAL_BASE);

	Map* map = circuit->GetMap();
	Mod* mod = circuit->GetCallback()->GetMod();
	int losMipLevel = mod->GetLosMipLevel();
	delete mod;

//	radarMap = map->GetRadarMap();
//	radarWidth = map->GetWidth() / 8;

	losMap = map->GetLosMap();
	losWidth = map->GetWidth() >> losMipLevel;
	losResConv = SQUARE_SIZE << losMipLevel;
}

CThreatMap::~CThreatMap()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);

#ifdef DEBUG_VIS
	if (sdlWindowId != DEBUG_MARK) {
		circuit->GetDebugDrawer()->DelSDLWindow(sdlWindowId);
		delete[] dbgMap;
	}
#endif
}

void CThreatMap::Update()
{
//	radarMap = circuit->GetMap()->GetRadarMap();
	losMap = circuit->GetMap()->GetLosMap();
	currMaxThreat = .0f;

	// account for moving units
	for (auto& kv : hostileUnits) {
		CEnemyUnit* e = kv.second;
		if (e->IsHidden()) {
			continue;
		}

		DelEnemyUnit(e);
//		if ((!e->IsInRadar() && IsInRadar(e->GetPos())) ||
//			(!e->IsInLOS() && IsInLOS(e->GetPos()))) {
		if (e->NotInRadarAndLOS() && IsInLOS(e->GetPos())) {
			e->SetHidden();
			continue;
		}
		if (e->IsInRadarOrLOS()) {
			AIFloat3 pos = e->GetUnit()->GetPos();
			circuit->GetTerrainManager()->CorrectPosition(pos);
			e->SetPos(pos);
		} else {
			e->DecayThreat(0.99f);  // decay 0.99^updateNum
		}
		if (e->IsInLOS()) {
			e->SetThreat(GetEnemyUnitThreat(e));
		}
		AddEnemyUnit(e);

		currMaxThreat = std::max(currMaxThreat, e->GetThreat());
	}

	for (auto& kv : peaceUnits) {
		CEnemyUnit* e = kv.second;
		if (e->IsHidden()) {
			continue;
		}
		if (e->NotInRadarAndLOS() && IsInLOS(e->GetPos())) {
			e->SetHidden();
			continue;
		}
		if (e->IsInRadarOrLOS()) {
			AIFloat3 pos = e->GetUnit()->GetPos();
			circuit->GetTerrainManager()->CorrectPosition(pos);
			e->SetPos(pos);
		}
	}

#ifdef DEBUG_VIS
	UpdateVis();
#endif
}

void CThreatMap::EnemyEnterLOS(CEnemyUnit* enemy)
{
	// Possible cases:
	// (1) Unknown enemy that has been detected for the first time
	// (2) Unknown enemy that was only in radar enters LOS
	// (3) Known enemy that already was in LOS enters again

	enemy->SetInLOS();

	if (enemy->GetDPS() < 0.1f) {
		if (enemy->GetThreat() > .0f) {  // (2)
			// threat prediction failed when enemy was unknown
			if (!enemy->IsHidden()) {
				DelEnemyUnit(enemy);
			}
			enemy->SetThreat(.0f);
			enemy->SetRange(.0f);
			hostileUnits.erase(enemy->GetId());
		}
		peaceUnits[enemy->GetId()] = enemy;
		AIFloat3 pos = enemy->GetUnit()->GetPos();
		circuit->GetTerrainManager()->CorrectPosition(pos);
		enemy->SetPos(pos);
		enemy->ClearHidden();
		return;
	}

	auto it = hostileUnits.find(enemy->GetId());
	if (it == hostileUnits.end()) {
		hostileUnits[enemy->GetId()] = enemy;
	} else if (enemy->IsHidden()) {
		enemy->ClearHidden();
	} else {
		DelEnemyUnit(enemy);
	}

	AIFloat3 pos = enemy->GetUnit()->GetPos();
	circuit->GetTerrainManager()->CorrectPosition(pos);
	enemy->SetPos(pos);
	enemy->SetThreat(GetEnemyUnitThreat(enemy));
	enemy->SetRange((enemy->GetUnit()->GetMaxRange() + 100.0f) / (SQUARE_SIZE * THREAT_RES));

	AddEnemyUnit(enemy);
}

void CThreatMap::EnemyLeaveLOS(CEnemyUnit* enemy)
{
	enemy->ClearInLOS();
}

void CThreatMap::EnemyEnterRadar(CEnemyUnit* enemy)
{
	// Possible cases:
	// (1) Unknown enemy wanders at radars
	// (2) Known enemy that once was in los wandering at radar
	// (3) EnemyEnterRadar invoked right after EnemyEnterLOS in area with no radar

	enemy->SetInRadar();

	if (enemy->IsInLOS()) {  // (3)
		return;
	}

	if (enemy->GetDPS() < 0.1f) {  // (2)
		AIFloat3 pos = enemy->GetUnit()->GetPos();
		circuit->GetTerrainManager()->CorrectPosition(pos);
		enemy->SetPos(pos);
		enemy->ClearHidden();
		return;
	}

	bool isNew = false;
	auto it = hostileUnits.find(enemy->GetId());
	if (it == hostileUnits.end()) {  // (1)
		std::tie(it, isNew) = hostileUnits.emplace(enemy->GetId(), enemy);
	} else if (enemy->IsHidden()) {
		enemy->ClearHidden();
	} else {
		DelEnemyUnit(enemy);
	}

	AIFloat3 pos = enemy->GetUnit()->GetPos();
	circuit->GetTerrainManager()->CorrectPosition(pos);
	enemy->SetPos(pos);
	if (isNew) {  // unknown enemy enters radar for the first time
		enemy->SetThreat(enemy->GetDPS());  // TODO: Randomize
		enemy->SetRange((150.0f + 100.0f) / (SQUARE_SIZE * THREAT_RES));
	}

	AddEnemyUnit(enemy);
}

void CThreatMap::EnemyLeaveRadar(CEnemyUnit* enemy)
{
	enemy->ClearInRadar();
}

void CThreatMap::EnemyDamaged(CEnemyUnit* enemy)
{
	auto it = hostileUnits.find(enemy->GetId());
	if ((it == hostileUnits.end()) || !enemy->IsInLOS()) {
		return;
	}

	DelEnemyUnit(enemy);
	enemy->SetThreat(GetEnemyUnitThreat(enemy));
	AddEnemyUnit(enemy);
}

void CThreatMap::EnemyDestroyed(CEnemyUnit* enemy)
{
	auto it = hostileUnits.find(enemy->GetId());
	if (it == hostileUnits.end()) {
		peaceUnits.erase(enemy->GetId());
		return;
	}

	if (!enemy->IsHidden()) {
		DelEnemyUnit(enemy);
	}
	hostileUnits.erase(it);
}

float CThreatMap::GetThreatAt(const AIFloat3& pos) const
{
	const int z = pos.z / (SQUARE_SIZE * THREAT_RES);
	const int x = pos.x / (SQUARE_SIZE * THREAT_RES);
	return threatCells[z * width + x] - THREAT_VAL_BASE;
}

float CThreatMap::GetUnitThreat(CCircuitUnit* unit) const
{
	return unit->GetDPS() * unit->GetUnit()->GetHealth() / unit->GetUnit()->GetMaxHealth();
}

void CThreatMap::AddEnemyUnit(const CEnemyUnit* e, const float scale)
{
	const int posx = e->GetPos().x / (SQUARE_SIZE * THREAT_RES);
	const int posz = e->GetPos().z / (SQUARE_SIZE * THREAT_RES);

	const float threat = e->GetThreat() * scale;
	const int rangeSq = e->GetRange() * e->GetRange();

	const int beginX = std::max(int(posx - e->GetRange()    ),      0);
	const int endX   = std::min(int(posx + e->GetRange() + 1),  width);
	const int beginZ = std::max(int(posz - e->GetRange()    ),      0);
	const int endZ   = std::min(int(posz + e->GetRange() + 1), height);

	for (int x = beginX; x < endX; ++x) {
		const int dxSq = (posx - x) * (posx - x);
		for (int z = beginZ; z < endZ; ++z) {
			const int dzSq = (posz - z) * (posz - z);

			if (dxSq + dzSq <= rangeSq) {
				// MicroPather cannot deal with negative costs
				// (which may arise due to floating-point drift)
				// nor with zero-cost nodes
				// (threat is not used as an additive overlay)
				threatCells[z * width + x] = std::max(threatCells[z * width + x] + threat, THREAT_VAL_BASE);

				currSumThreat += threat;
			}
		}
	}

	currAvgThreat = currSumThreat / threatCells.size();
}

float CThreatMap::GetEnemyUnitThreat(CEnemyUnit* enemy) const
{
	const float dps = std::min(enemy->GetDPS(), 2000.0f);
	const float dpsMod = std::max(enemy->GetUnit()->GetHealth(), .0f) / enemy->GetUnit()->GetMaxHealth();
	return dps * dpsMod;
}

bool CThreatMap::IsInLOS(const AIFloat3& pos) const
{
	// res = 1 << Mod->GetLosMipLevel();
	// the value for the full resolution position (x, z) is at index ((z * width + x) / res)
	// the last value, bottom right, is at index (width/res * height/res - 1)

	// convert from world coordinates to losmap coordinates
	const int x = pos.x / losResConv;
	const int z = pos.z / losResConv;
	return losMap[z * losWidth + x] > 0;
}

//bool CThreatMap::IsInRadar(const AIFloat3& pos) const
//{
//	// the value for the full resolution position (x, z) is at index ((z * width + x) / 8)
//	// the last value, bottom right, is at index (width/8 * height/8 - 1)
//
//	// convert from world coordinates to radarmap coordinates
//	const int x = pos.x / (SQUARE_SIZE * 8);
//	const int z = pos.z / (SQUARE_SIZE * 8);
//	return radarMap[z * radarWidth + x] > 0;
//}

#ifdef DEBUG_VIS
void CThreatMap::UpdateVis()
{
	if ((sdlWindowId != DEBUG_MARK)/* && (currMaxThreat > .0f)*/) {
		for (int i = 0; i < threatCells.size(); ++i) {
			dbgMap[i] = std::min((threatCells[i] - THREAT_VAL_BASE) / 200.0f /*currMaxThreat*/, 1.0f);
		}
		circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap);
	}
}

void CThreatMap::ToggleVis()
{
	if (sdlWindowId == DEBUG_MARK) {
		// ~threat
		dbgMap = new float [threatCells.size()];
		std::string label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Threat Map");
		sdlWindowId = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		UpdateVis();
	} else {
		circuit->GetDebugDrawer()->DelSDLWindow(sdlWindowId);
		sdlWindowId = DEBUG_MARK;
		delete[] dbgMap;
	}
}
#endif

} // namespace circuit
