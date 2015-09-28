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
#include "Map.h"

namespace circuit {

using namespace springai;

#define THREAT_VAL_BASE		1.0f

CThreatMap::CThreatMap(CCircuitAI* circuit)
		: circuit(circuit)
//		, currMaxThreat(.0f)  // maximum threat (normalizer)
//		, currSumThreat(.0f)  // threat summed over all cells
//		, currAvgThreat(.0f)  // average threat over all cells
{
	squareSize = circuit->GetTerrainManager()->GetConvertStoP();
	width = circuit->GetTerrainManager()->GetTerrainWidth() / squareSize;
	height = circuit->GetTerrainManager()->GetTerrainHeight() / squareSize;

	rangeDefault = (DEFAULT_SLACK * 4) / squareSize;
	distCloak = (DEFAULT_SLACK * 3) / squareSize;

	airThreat.resize(width * height, THREAT_VAL_BASE);
	landThreat.resize(width * height, THREAT_VAL_BASE);
	waterThreat.resize(width * height, THREAT_VAL_BASE);
	cloakThreat.resize(width * height, THREAT_VAL_BASE);
	pthreats = &landThreat;

	Map* map = circuit->GetMap();
	Mod* mod = circuit->GetCallback()->GetMod();
	int losMipLevel = mod->GetLosMipLevel();
	delete mod;

//	radarMap = std::move(map->GetRadarMap());
//	radarWidth = map->GetWidth() / 8;

	losMap = std::move(map->GetLosMap());
	losWidth = map->GetWidth() >> losMipLevel;
	losResConv = SQUARE_SIZE << losMipLevel;
}

CThreatMap::~CThreatMap()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);

#ifdef DEBUG_VIS
	for (const std::pair<Uint32, float*>& win : sdlWindows) {
		circuit->GetDebugDrawer()->DelSDLWindow(win.first);
		delete[] win.second;
	}
#endif
}

void CThreatMap::Update()
{
//	radarMap = std::move(circuit->GetMap()->GetRadarMap());
	losMap = std::move(circuit->GetMap()->GetLosMap());
//	currMaxThreat = .0f;

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
//		} else {
//			e->DecayThreat(0.99f);  // decay 0.99^updateNum
		}
		if (e->IsInLOS()) {
			e->SetThreat(GetEnemyUnitThreat(e));
		}
		AddEnemyUnit(e);

//		currMaxThreat = std::max(currMaxThreat, e->GetThreat());
	}

	for (auto& kv : peaceUnits) {
		CEnemyUnit* e = kv.second;
		if (e->IsHidden()) {
			continue;
		}
		if (e->NotInRadarAndLOS() && IsInLOS(e->GetPos())) {
			DelDecloaker(e);
			e->SetHidden();
			continue;
		}
		if (e->IsInRadarOrLOS()) {
			AIFloat3 pos = e->GetUnit()->GetPos();
			circuit->GetTerrainManager()->CorrectPosition(pos);
			if (pos != e->GetPos()) {
				DelDecloaker(e);
				e->SetPos(pos);
				AddDecloaker(e);
			}
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
			if (enemy->IsHidden()) {
				enemy->ClearHidden();
			} else {
				DelEnemyUnitAll(enemy);
			}
			enemy->SetThreat(.0f);
			enemy->SetRange(0);
			enemy->SetDecloakRange(GetCloakRange(enemy));
			hostileUnits.erase(enemy->GetId());
			peaceUnits[enemy->GetId()] = enemy;
		} else if (peaceUnits.find(enemy->GetId()) == peaceUnits.end()) {
			enemy->SetDecloakRange(GetCloakRange(enemy));
			peaceUnits[enemy->GetId()] = enemy;
		} else if (enemy->IsHidden()) {
			enemy->ClearHidden();
		} else {
			DelDecloaker(enemy);
		}

		AIFloat3 pos = enemy->GetUnit()->GetPos();
		circuit->GetTerrainManager()->CorrectPosition(pos);
		enemy->SetPos(pos);

		AddDecloaker(enemy);
		return;
	}

	if (hostileUnits.find(enemy->GetId()) == hostileUnits.end()) {
		hostileUnits[enemy->GetId()] = enemy;
	} else if (enemy->IsHidden()) {
		enemy->ClearHidden();
	} else {
		DelEnemyUnit(enemy);
	}

	AIFloat3 pos = enemy->GetUnit()->GetPos();
	circuit->GetTerrainManager()->CorrectPosition(pos);
	enemy->SetPos(pos);
	enemy->SetRange(GetEnemyUnitRange(enemy));
	enemy->SetDecloakRange(GetCloakRange(enemy));
	enemy->SetThreat(GetEnemyUnitThreat(enemy));

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
		if (enemy->IsHidden()) {
			enemy->ClearHidden();
		} else {
			DelDecloaker(enemy);
		}

		AIFloat3 pos = enemy->GetUnit()->GetPos();
		circuit->GetTerrainManager()->CorrectPosition(pos);
		enemy->SetPos(pos);

		AddDecloaker(enemy);
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
		enemy->SetRange(rangeDefault);
		enemy->SetDecloakRange(distCloak);
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
		if (!enemy->IsHidden()) {
			DelDecloaker(enemy);
		}
		peaceUnits.erase(enemy->GetId());
		return;
	}

	if (!enemy->IsHidden()) {
		DelEnemyUnit(enemy);
	}
	hostileUnits.erase(it);
}

float CThreatMap::GetAllThreatAt(const AIFloat3& position) const
{
	const int z = position.z / squareSize;
	const int x = position.x / squareSize;
	const int index = z * width + x;
//	float air = airThreat[index] - THREAT_VAL_BASE;
	float land = landThreat[index] - THREAT_VAL_BASE;
	float water = waterThreat[index] - THREAT_VAL_BASE;
	return /*air + */land + water;
}

void CThreatMap::SetThreatType(CCircuitUnit* unit)
{
	if (unit->GetCircuitDef()->GetUnitDef()->IsAbleToFly()) {
		pthreats = &airThreat;
	} else if (unit->GetUnit()->GetPos().y < -10.0f) {
		pthreats = &waterThreat;
	} else {
		pthreats = &landThreat;
	}
}

float CThreatMap::GetThreatAt(const AIFloat3& position) const
{
	const int z = position.z / squareSize;
	const int x = position.x / squareSize;
	return (*pthreats)[z * width + x] - THREAT_VAL_BASE;
}

float CThreatMap::GetThreatAt(CCircuitUnit* unit, const AIFloat3& position) const
{
	const int z = position.z / squareSize;
	const int x = position.x / squareSize;
	if (unit->GetCircuitDef()->GetUnitDef()->IsAbleToFly()) {
		return airThreat[z * width + x] - THREAT_VAL_BASE;
	}
	if (unit->GetUnit()->GetPos().y < -10.0f) {
		return waterThreat[z * width + x] - THREAT_VAL_BASE;
	}
	return landThreat[z * width + x] - THREAT_VAL_BASE;
}

float CThreatMap::GetUnitThreat(CCircuitUnit* unit) const
{
	return unit->GetDPS() * unit->GetUnit()->GetHealth()/* / unit->GetUnit()->GetMaxHealth()*/;
}

void CThreatMap::AddEnemyUnit(const CEnemyUnit* e, const float scale)
{
	CCircuitDef* cdef = e->GetCircuitDef();
	if (cdef == nullptr) {
		AddEnemyUnitAll(e, scale);
		return;
	}

	if (cdef->IsAntiAir()) {
		AddEnemyUnit(e, airThreat, scale);
	}
	if (cdef->IsAntiLand()) {
		AddEnemyUnit(e, landThreat, scale);
	}
	if (cdef->IsAntiWater()) {
		AddEnemyUnit(e, waterThreat, scale);
	}
	AddDecloaker(e, scale);
}

void CThreatMap::AddEnemyUnitAll(const CEnemyUnit* e, const float scale)
{
	const int posx = (int)e->GetPos().x / squareSize;
	const int posz = (int)e->GetPos().z / squareSize;

	const float threat = e->GetThreat() * scale;
	const float threatCloak = 10.0f * scale;
	const int rangeSq = e->GetRange() * e->GetRange();
	const int rangeCloakSq = e->GetDecloakRange() * e->GetDecloakRange();

	const int beginX = std::max(int(posx - e->GetRange()    ),      0);
	const int endX   = std::min(int(posx + e->GetRange() + 1),  width);
	const int beginZ = std::max(int(posz - e->GetRange()    ),      0);
	const int endZ   = std::min(int(posz + e->GetRange() + 1), height);

	for (int x = beginX; x < endX; ++x) {
		const int dxSq = (posx - x) * (posx - x);
		for (int z = beginZ; z < endZ; ++z) {
			const int dzSq = (posz - z) * (posz - z);

			if (dxSq + dzSq <= rangeSq) {
				const int index = z * width + x;
				airThreat[index]   = std::max(airThreat[index]   + threat, THREAT_VAL_BASE);
				landThreat[index]  = std::max(landThreat[index]  + threat, THREAT_VAL_BASE);
				waterThreat[index] = std::max(waterThreat[index] + threat, THREAT_VAL_BASE);

				if (dxSq + dzSq <= rangeCloakSq) {  // Assuming decloak range <= weapon range
					cloakThreat[index] = std::max(cloakThreat[index] + threatCloak, THREAT_VAL_BASE);
				}
			}
		}
	}
}

void CThreatMap::AddEnemyUnit(const CEnemyUnit* e, Threats& threats, const float scale)
{
	const int posx = (int)e->GetPos().x / squareSize;
	const int posz = (int)e->GetPos().z / squareSize;

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
				const int index = z * width + x;
				// MicroPather cannot deal with negative costs
				// (which may arise due to floating-point drift)
				// nor with zero-cost nodes (see MP::SetMapData,
				// threat is not used as an additive overlay)
				threats[index] = std::max(threats[index] + threat, THREAT_VAL_BASE);

//				currSumThreat += threat;
			}
		}
	}

//	currAvgThreat = currSumThreat / landThreat.size();
}

void CThreatMap::AddDecloaker(const CEnemyUnit* e, const float scale)
{
	const int posx = (int)e->GetPos().x / squareSize;
	const int posz = (int)e->GetPos().z / squareSize;

	const float threatCloak = 10.0f * scale;
	const int rangeCloakSq = e->GetDecloakRange() * e->GetDecloakRange();

	const int beginX = std::max(int(posx - e->GetDecloakRange()    ),      0);
	const int endX   = std::min(int(posx + e->GetDecloakRange() + 1),  width);
	const int beginZ = std::max(int(posz - e->GetDecloakRange()    ),      0);
	const int endZ   = std::min(int(posz + e->GetDecloakRange() + 1), height);

	for (int x = beginX; x < endX; ++x) {
		const int dxSq = (posx - x) * (posx - x);
		for (int z = beginZ; z < endZ; ++z) {
			const int dzSq = (posz - z) * (posz - z);

			if (dxSq + dzSq <= rangeCloakSq) {
				const int index = z * width + x;
				cloakThreat[index] = std::max(cloakThreat[index] + threatCloak, THREAT_VAL_BASE);
			}
		}
	}
}

int CThreatMap::GetEnemyUnitRange(const CEnemyUnit* e) const
{
	assert(e->GetCircuitDef() != nullptr);
	int range = e->GetUnit()->GetMaxRange();
	if (e->GetCircuitDef()->GetUnitDef()->GetSpeed() > 0) {
		return (range + DEFAULT_SLACK * 4) / squareSize;
	}
	return (range + DEFAULT_SLACK * 2) / squareSize;
}

int CThreatMap::GetCloakRange(const CEnemyUnit* e) const
{
	assert(e->GetCircuitDef() != nullptr);
	const int sizeX = e->GetCircuitDef()->GetUnitDef()->GetXSize() * (SQUARE_SIZE / 2);
	const int sizeZ = e->GetCircuitDef()->GetUnitDef()->GetZSize() * (SQUARE_SIZE / 2);
	return (int)sqrtf(sizeX * sizeX + sizeZ * sizeZ) / squareSize + distCloak;
}

float CThreatMap::GetEnemyUnitThreat(CEnemyUnit* enemy) const
{
	if (enemy->GetRange() > 2000 / squareSize) {
		return THREAT_VAL_BASE;  // or 0
	}
	const float dps = std::min(enemy->GetDPS(), 2000.0f);
	const float dpsMod = std::max(enemy->GetUnit()->GetHealth(), .0f)/* / enemy->GetUnit()->GetMaxHealth()*/;
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
	if (sdlWindows.empty()/* || (currMaxThreat < .1f)*/) {
		return;
	}

	Uint32 sdlWindowId;
	float* dbgMap;
	std::tie(sdlWindowId, dbgMap) = sdlWindows[0];
	for (int i = 0; i < airThreat.size(); ++i) {
		dbgMap[i] = std::min((airThreat[i] - THREAT_VAL_BASE) / 40000.0f /*currMaxThreat*/, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap);

	std::tie(sdlWindowId, dbgMap) = sdlWindows[1];
	for (int i = 0; i < landThreat.size(); ++i) {
		dbgMap[i] = std::min((landThreat[i] - THREAT_VAL_BASE) / 40000.0f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap);

	std::tie(sdlWindowId, dbgMap) = sdlWindows[2];
	for (int i = 0; i < waterThreat.size(); ++i) {
		dbgMap[i] = std::min((waterThreat[i] - THREAT_VAL_BASE) / 40000.0f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap);

	std::tie(sdlWindowId, dbgMap) = sdlWindows[3];
	for (int i = 0; i < cloakThreat.size(); ++i) {
		dbgMap[i] = std::min((cloakThreat[i] - THREAT_VAL_BASE) / 10.0f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap);
}

void CThreatMap::ToggleVis()
{
	if (sdlWindows.empty()) {
		// ~threat
		std::pair<Uint32, float*> win;

		win.second = new float [airThreat.size()];
		std::string label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: AIR Threat Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [landThreat.size()];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: LAND Threat Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [waterThreat.size()];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: WATER Threat Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [cloakThreat.size()];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: CLOAK Threat Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		UpdateVis();
	} else {
		for (const std::pair<Uint32, float*> win : sdlWindows) {
			circuit->GetDebugDrawer()->DelSDLWindow(win.first);
			delete[] win.second;
		}
		sdlWindows.clear();
	}
}
#endif

} // namespace circuit
