/*
 * ThreatMap.cpp
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 *      Original implementation: https://github.com/spring/KAIK/blob/master/ThreatMap.cpp
 */

#include "terrain/ThreatMap.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
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
	}
	delete[] dbgMap;
#endif
}

void CThreatMap::Update()
{
//	radarMap = circuit->GetMap()->GetRadarMap();
	losMap = circuit->GetMap()->GetLosMap();
	currMaxThreat = .0f;

	// account for moving units
	for (auto& kv : enemyUnits) {
		SEnemyUnit& e = kv.second;
		if (e.losStatus & LosType::HIDDEN) {
			continue;
		}

		DelEnemyUnit(e);
//		if ((((e.losStatus & LosType::RADAR) == 0) && IsInRadar(e.pos)) ||
//			(((e.losStatus & LosType::LOS) == 0) && IsInLOS(e.pos))) {
		if (((e.losStatus & ((LosType::RADAR | LosType::LOS))) == 0) && IsInLOS(e.pos)) {
			e.losStatus |= LosType::HIDDEN;
			continue;
		}
		if (e.losStatus & (LosType::RADAR | LosType::LOS)) {
			e.pos = e.unit->GetUnit()->GetPos();
		} else {
			e.threat *= 0.99f;  // decay 0.99^updateNum
		}
		if (e.losStatus & LosType::LOS) {
			e.threat = GetEnemyUnitThreat(e.unit);
		}
		AddEnemyUnit(e);

		currMaxThreat = std::max(currMaxThreat, e.threat);
	}

#ifdef DEBUG_VIS
	UpdateVis();
#endif
}

void CThreatMap::EnemyEnterLOS(CCircuitUnit* enemy)
{
	if (enemy->GetDPS() < 0.1f) {
		return;
	}

	auto it = enemyUnits.find(enemy->GetId());
	if (it == enemyUnits.end()) {
		bool ok;
		std::tie(it, ok) = enemyUnits.emplace(enemy->GetId(), SEnemyUnit(enemy));
	} else if (it->second.losStatus & LosType::HIDDEN) {
		it->second.losStatus &= ~LosType::HIDDEN;
	} else {
		DelEnemyUnit(it->second);
	}

	SEnemyUnit& e = it->second;
	e.pos = enemy->GetUnit()->GetPos();
	e.threat = GetEnemyUnitThreat(enemy);
	e.range = (enemy->GetUnit()->GetMaxRange() + 100.0f) / (SQUARE_SIZE * THREAT_RES);
	e.losStatus |= LosType::LOS;

	circuit->GetTerrainManager()->CorrectPosition(e.pos);
	AddEnemyUnit(e);
}

void CThreatMap::EnemyLeaveLOS(CCircuitUnit* enemy)
{
	auto it = enemyUnits.find(enemy->GetId());
	if (it == enemyUnits.end()) {
		return;
	}

	SEnemyUnit& e = it->second;
	e.losStatus &= ~LosType::LOS;
}

void CThreatMap::EnemyEnterRadar(CCircuitUnit* enemy)
{
	if (enemy->GetDPS() < 0.1f) {
		return;
	}

	bool isNew = false;
	auto it = enemyUnits.find(enemy->GetId());
	if (it == enemyUnits.end()) {
		std::tie(it, isNew) = enemyUnits.emplace(enemy->GetId(), SEnemyUnit(enemy));
	} else if (it->second.losStatus & LosType::LOS) {
		it->second.losStatus |= LosType::RADAR;
		return;
	} else if (it->second.losStatus & LosType::HIDDEN) {
		it->second.losStatus &= ~LosType::HIDDEN;
	} else {
		DelEnemyUnit(it->second);
	}

	SEnemyUnit& e = it->second;
	e.pos = enemy->GetUnit()->GetPos();
	if (isNew) {  // unknown enemy enters radar for the first time
		e.threat = enemy->GetDPS();  // TODO: Randomize
		e.range = (150.0f + 100.0f) / (SQUARE_SIZE * THREAT_RES);
	}
	e.losStatus |= LosType::RADAR;

	circuit->GetTerrainManager()->CorrectPosition(e.pos);
	AddEnemyUnit(e);
}

void CThreatMap::EnemyLeaveRadar(CCircuitUnit* enemy)
{
	auto it = enemyUnits.find(enemy->GetId());
	if (it == enemyUnits.end()) {
		return;
	}

	SEnemyUnit& e = it->second;
	e.losStatus &= ~LosType::RADAR;
}

void CThreatMap::EnemyDamaged(CCircuitUnit* enemy)
{
	auto it = enemyUnits.find(enemy->GetId());
	if (it == enemyUnits.end()) {
		return;
	}

	SEnemyUnit& e = it->second;
	if (e.losStatus & LosType::LOS) {
		DelEnemyUnit(e);
		e.threat = GetEnemyUnitThreat(enemy);
		AddEnemyUnit(e);
	}
}

void CThreatMap::EnemyDestroyed(CCircuitUnit* enemy)
{
	auto it = enemyUnits.find(enemy->GetId());
	if (it == enemyUnits.end()) {
		return;
	}

	const SEnemyUnit& e = it->second;
	if ((e.losStatus & LosType::HIDDEN) == 0) {
		DelEnemyUnit(e);
	}
	enemyUnits.erase(it);
}

float CThreatMap::GetThreatAt(const AIFloat3& pos) const
{
	const int z = pos.z / (SQUARE_SIZE * THREAT_RES);
	const int x = pos.x / (SQUARE_SIZE * THREAT_RES);
	return threatCells[z * width + x] - THREAT_VAL_BASE;
}

void CThreatMap::AddEnemyUnit(const SEnemyUnit& e, const float scale)
{
	const int posx = e.pos.x / (SQUARE_SIZE * THREAT_RES);
	const int posz = e.pos.z / (SQUARE_SIZE * THREAT_RES);

	const float threat = e.threat * scale;
	const int rangeSq = e.range * e.range;

	const int beginX = std::max(int(posx - e.range), 0);
	const int endX = std::min(int(posx + e.range + 1), width);
	const int beginZ = std::max(int(posz - e.range), 0);
	const int endZ = std::min(int(posz + e.range + 1), height);

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

float CThreatMap::GetEnemyUnitThreat(CCircuitUnit* enemy) const
{
	Unit* u = enemy->GetUnit();
	if (u->IsBeingBuilt()) {
		return .0f;
	}
	const float dps = std::min(enemy->GetDPS(), 2000.0f);
	const float dpsMod = std::max(u->GetHealth(), .0f) / u->GetMaxHealth();
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
