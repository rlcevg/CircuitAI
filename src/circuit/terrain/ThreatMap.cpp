/*
 * ThreatMap.cpp
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 */

#include "terrain/ThreatMap.h"
#include "terrain/TerrainManager.h"
#include "setup/SetupManager.h"
#include "unit/CircuitUnit.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#include "json/json.h"

#include "OOAICallback.h"
#include "Mod.h"
#include "Map.h"
#ifdef DEBUG_VIS
#include "Lua.h"
#endif

//#undef NDEBUG
#include <cassert>

namespace circuit {

using namespace springai;

#define THREAT_BASE		0.f
#define THREAT_DECAY	1e-2f
#define THREAT_CLOAK	16.0f
#define VEL_EPSILON		1e-2f

CThreatMap::CThreatMap(CCircuitAI* circuit, float decloakRadius)
		: circuit(circuit)
		, pThreatData(&threatData0)
		, isUpdating(false)
{
	areaData = circuit->GetTerrainManager()->GetAreaData();
	squareSize = circuit->GetTerrainManager()->GetConvertStoP();
	width = circuit->GetTerrainManager()->GetSectorXSize();
	height = circuit->GetTerrainManager()->GetSectorZSize();
	mapSize = width * height;

	rangeDefault = (DEFAULT_SLACK * 4) / squareSize;
	distCloak = (decloakRadius + DEFAULT_SLACK) / squareSize;

	threatData0.airThreat.resize(mapSize, THREAT_BASE);
	threatData0.surfThreat.resize(mapSize, THREAT_BASE);
	threatData0.amphThreat.resize(mapSize, THREAT_BASE);
	threatData0.cloakThreat.resize(mapSize, THREAT_BASE);
	threatData0.shield.resize(mapSize, 0.f);
	airThreat = threatData0.airThreat.data();
	surfThreat = threatData0.surfThreat.data();
	amphThreat = threatData0.amphThreat.data();
	cloakThreat = threatData0.cloakThreat.data();
	shieldArray = threatData0.shield.data();
	threatArray = surfThreat;

	threatData1.airThreat.resize(mapSize, THREAT_BASE);
	threatData1.surfThreat.resize(mapSize, THREAT_BASE);
	threatData1.amphThreat.resize(mapSize, THREAT_BASE);
	threatData1.cloakThreat.resize(mapSize, THREAT_BASE);
	threatData1.shield.resize(mapSize, 0.f);
	drawAirThreat = threatData1.airThreat.data();
	drawSurfThreat = threatData1.surfThreat.data();
	drawAmphThreat = threatData1.amphThreat.data();
	drawCloakThreat = threatData1.cloakThreat.data();
	drawShieldArray = threatData1.shield.data();

	Map* map = circuit->GetMap();
	int mapWidth = map->GetWidth();
	Mod* mod = circuit->GetCallback()->GetMod();
	int losMipLevel = mod->GetLosMipLevel();
	int radarMipLevel = mod->GetRadarMipLevel();
	delete mod;

//	radarMap = std::move(map->GetRadarMap());
	radarWidth = mapWidth >> radarMipLevel;
	sonarMap = std::move(map->GetSonarMap());
	radarResConv = SQUARE_SIZE << radarMipLevel;
	losMap = std::move(map->GetLosMap());
	losWidth = mapWidth >> losMipLevel;
	losResConv = SQUARE_SIZE << losMipLevel;

	// FIXME: DEBUG
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	slackMod = root["quota"].get("slack_mod", 2.f).asFloat();
	constexpr float allowedRange = 2000.f;
	for (auto& kv : circuit->GetCircuitDefs()) {
		CCircuitDef* cdef = kv.second;
		const float velSlack = DEFAULT_SLACK * slackMod + (cdef->IsMobile() ? cdef->GetSpeed() / FRAMES_PER_SEC * THREAT_UPDATE_RATE : 0.f);
		// TODO: slack should also depend on own unit's path update rate
		const float slack = squareSize - 1 + cdef->GetAoe();
		float realRange;
		int range;
		int maxRange;

		realRange = cdef->GetMaxRange(CCircuitDef::RangeType::AIR);
		range = cdef->HasAntiAir() ? int(realRange + slack) / squareSize + 1 : 0;
		cdef->SetThreatRange(CCircuitDef::ThreatType::AIR, range);
		range = cdef->HasAntiAir() ? int(realRange + slack + velSlack) / squareSize + 1 : 0;
		cdef->SetThreatRange(CCircuitDef::ThreatType::VEL_AIR, range);
		maxRange = range;

		realRange = cdef->GetMaxRange(CCircuitDef::RangeType::LAND);
		range = (cdef->HasAntiLand() && (realRange <= allowedRange)) ? int(realRange + slack) / squareSize + 1 : 0;
		cdef->SetThreatRange(CCircuitDef::ThreatType::LAND, range);
		range = (cdef->HasAntiLand() && (realRange <= allowedRange)) ? int(realRange + slack + velSlack) / squareSize + 1 : 0;
		cdef->SetThreatRange(CCircuitDef::ThreatType::VEL_LAND, range);
		maxRange = std::max(maxRange, range);

		realRange = cdef->GetMaxRange(CCircuitDef::RangeType::WATER);
		range = (cdef->HasAntiWater() && (realRange <= allowedRange)) ? int(realRange + slack) / squareSize + 1 : 0;
		cdef->SetThreatRange(CCircuitDef::ThreatType::WATER, range);
		range = (cdef->HasAntiWater() && (realRange <= allowedRange)) ? int(realRange + slack + velSlack) / squareSize + 1 : 0;
		cdef->SetThreatRange(CCircuitDef::ThreatType::VEL_WATER, range);
		maxRange = std::max(maxRange, range);

		cdef->SetThreatRange(CCircuitDef::ThreatType::MAX, maxRange);
		cdef->SetThreatRange(CCircuitDef::ThreatType::CLOAK, GetCloakRange(cdef));
		cdef->SetThreatRange(CCircuitDef::ThreatType::SHIELD, GetShieldRange(cdef));
	}
	// FIXME: DEBUG
}

CThreatMap::~CThreatMap()
{
#ifdef DEBUG_VIS
	for (const std::pair<Uint32, float*>& win : sdlWindows) {
		circuit->GetDebugDrawer()->DelSDLWindow(win.first);
		delete[] win.second;
	}
#endif
}

void CThreatMap::EnqueueUpdate()
{
	if (isUpdating) {
		return;
	}
	isUpdating = true;

//	radarMap = std::move(circuit->GetMap()->GetRadarMap());
	sonarMap = std::move(circuit->GetMap()->GetSonarMap());
	losMap   = std::move(circuit->GetMap()->GetLosMap());

	areaData = circuit->GetTerrainManager()->GetAreaData();

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
			e->SetThreat(GetEnemyUnitThreat(e));
		}

		hostileDatas.push_back(e->GetData());
	}

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

	circuit->GetScheduler()->RunParallelTask(std::make_shared<CGameTask>(&CThreatMap::Update, this),
											 std::make_shared<CGameTask>(&CThreatMap::Apply, this));
}

bool CThreatMap::EnemyEnterLOS(CEnemyUnit* enemy)
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
			SetEnemyUnitRange(enemy);
		} else if (peaceUnits.find(enemy->GetId()) == peaceUnits.end()) {
			peaceUnits[enemy->GetId()] = enemy;
			SetEnemyUnitRange(enemy);
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
	SetEnemyUnitRange(enemy);
	enemy->SetThreat(GetEnemyUnitThreat(enemy));
	enemy->SetKnown();

	return !wasKnown;
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
		enemy->SetThreat(enemy->GetDamage());  // TODO: Randomize
		enemy->SetRange(CCircuitDef::ThreatType::MAX, rangeDefault);
		enemy->SetRange(CCircuitDef::ThreatType::AIR, rangeDefault);
		enemy->SetRange(CCircuitDef::ThreatType::LAND, rangeDefault);
		enemy->SetRange(CCircuitDef::ThreatType::WATER, rangeDefault);
		enemy->SetRange(CCircuitDef::ThreatType::CLOAK, distCloak);
//		enemy->SetRange(CCircuitDef::ThreatType::SHIELD, 0);
	}
}

void CThreatMap::EnemyLeaveRadar(CEnemyUnit* enemy)
{
	enemy->ClearInRadar();
}

bool CThreatMap::EnemyDestroyed(CEnemyUnit* enemy)
{
	auto it = hostileUnits.find(enemy->GetId());
	if (it == hostileUnits.end()) {
		peaceUnits.erase(enemy->GetId());
		return enemy->IsKnown();
	}

	hostileUnits.erase(it);
	return enemy->IsKnown();
}

float CThreatMap::GetAllThreatAt(const AIFloat3& position) const
{
	assert((position.x >= 0) && (position.x < CTerrainManager::GetTerrainWidth()) &&
		   (position.z >= 0) && (position.z < CTerrainManager::GetTerrainHeight()));
	int x, z;
	PosToXZ(position, x, z);
	const int index = z * width + x;
//	float air = airThreat[index] - THREAT_BASE;
	float land = surfThreat[index] - THREAT_BASE;
//	float water = amphThreat[index] - THREAT_BASE;
	return /*air + */land/* + water*/;
}

void CThreatMap::SetThreatType(CCircuitUnit* unit)
{
	assert(unit != nullptr);
	if (unit->GetCircuitDef()->IsAbleToFly()) {
		threatArray = airThreat;
//	} else if (unit->GetPos(circuit->GetLastFrame()).y < -SQUARE_SIZE * 5) {
	} else if (unit->GetCircuitDef()->IsAmphibious()) {
		threatArray = amphThreat;
	} else {
		threatArray = surfThreat;
	}
}

float CThreatMap::GetThreatAt(const AIFloat3& position) const
{
	assert((position.x >= 0) && (position.x < CTerrainManager::GetTerrainWidth()) &&
		   (position.z >= 0) && (position.z < CTerrainManager::GetTerrainHeight()));
	int x, z;
	PosToXZ(position, x, z);
	return threatArray[z * width + x] - THREAT_BASE;
}

float CThreatMap::GetThreatAt(CCircuitUnit* unit, const AIFloat3& position) const
{
	assert(unit != nullptr);
	assert((position.x >= 0) && (position.x < CTerrainManager::GetTerrainWidth()) &&
		   (position.z >= 0) && (position.z < CTerrainManager::GetTerrainHeight()));
	int x, z;
	PosToXZ(position, x, z);
	if (unit->GetCircuitDef()->IsAbleToFly()) {
		return airThreat[z * width + x] - THREAT_BASE;
	}
//	if (unit->GetPos(circuit->GetLastFrame()).y < -SQUARE_SIZE * 5) {
	if (unit->GetCircuitDef()->IsAmphibious()) {
		return amphThreat[z * width + x] - THREAT_BASE;
	}
	return surfThreat[z * width + x] - THREAT_BASE;
}

float CThreatMap::GetUnitThreat(CCircuitUnit* unit) const
{
	float health = unit->GetUnit()->GetHealth() + unit->GetShieldPower() * 2.0f;
	return unit->GetDamage() * sqrtf(std::max(health, 0.f));  // / unit->GetUnit()->GetMaxHealth();
}

inline void CThreatMap::PosToXZ(const AIFloat3& pos, int& x, int& z) const
{
	x = (int)pos.x / squareSize;
	z = (int)pos.z / squareSize;
}

inline AIFloat3 CThreatMap::XZToPos(int x, int z) const
{
	AIFloat3 pos;
	pos.z = z * squareSize + squareSize / 2;
	pos.x = x * squareSize + squareSize / 2;
	return pos;
}

void CThreatMap::Prepare(SThreatData& threatData)
{
	drawAirThreat = threatData.airThreat.data();
	drawSurfThreat = threatData.surfThreat.data();
	drawAmphThreat = threatData.amphThreat.data();
	drawCloakThreat = threatData.cloakThreat.data();
	drawShieldArray = threatData.shield.data();

	std::fill(threatData.airThreat.begin(), threatData.airThreat.end(), THREAT_BASE);
	std::fill(threatData.surfThreat.begin(), threatData.surfThreat.end(), THREAT_BASE);
	std::fill(threatData.amphThreat.begin(), threatData.amphThreat.end(), THREAT_BASE);
	std::fill(threatData.cloakThreat.begin(), threatData.cloakThreat.end(), THREAT_BASE);
	std::fill(threatData.shield.begin(), threatData.shield.end(), 0.f);
}

void CThreatMap::AddEnemyUnit(const CEnemyUnit::SData& e)
{
	CCircuitDef* cdef = e.cdef;
	if (cdef == nullptr) {
		AddEnemyUnitAll(e);
		return;
	}

	if (cdef->HasAntiAir()) {
		AddEnemyAir(e);
	}
	if (cdef->HasAntiLand() || cdef->HasAntiWater()) {
		cdef->IsAlwaysHit() ? AddEnemyAmphConst(e) : AddEnemyAmphGradient(e);
	}
	AddDecloaker(e);

	if (cdef->GetShieldMount() != nullptr) {
		AddShield(e);
	}
}

void CThreatMap::AddEnemyUnitAll(const CEnemyUnit::SData& e)
{
	AddEnemyAir(e);
	AddEnemyAmphGradient(e);
	AddDecloaker(e);
}

void CThreatMap::AddEnemyAir(const CEnemyUnit::SData& e)
{
	int posx, posz;
	PosToXZ(e.pos, posx, posz);

	const float threat = e.threat/* - THREAT_DECAY*/;
	const int range = CEnemyUnit::GetRange(e.range, (e.vel.SqLength2D() < VEL_EPSILON) ?
			CCircuitDef::ThreatType::AIR : CCircuitDef::ThreatType::VEL_AIR);
	const int rangeSq = SQUARE(range);

	// Threat circles are large and often have appendix, decrease it by 1 for micro-optimization
	const int beginX = std::max(int(posx - range + 1),      0);
	const int endX   = std::min(int(posx + range    ),  width);
	const int beginZ = std::max(int(posz - range + 1),      0);
	const int endZ   = std::min(int(posz + range    ), height);

	for (int z = beginZ; z < endZ; ++z) {
		const int dzSq = SQUARE(posz - z);
		for (int x = beginX; x < endX; ++x) {
			const int dxSq = SQUARE(posx - x);
			const int sum = dxSq + dzSq;
			if (sum > rangeSq) {
				continue;
			}

			const int index = z * width + x;
			const float heat = threat * (2.0f - 1.0f * sqrtf(sum) / range);
			// MicroPather cannot deal with negative costs
			// (which may arise due to floating-point drift)
			// nor with zero-cost nodes (see MP::SetMapData,
			// threat is not used as an additive overlay)
			drawAirThreat[index] += heat;
		}
	}
}

void CThreatMap::AddEnemyAmphConst(const CEnemyUnit::SData& e)
{
	int posx, posz;
	PosToXZ(e.pos, posx, posz);

	const float threat = e.threat/* - THREAT_DECAY*/;
	const int rangeLand = CEnemyUnit::GetRange(e.range, (e.vel.SqLength2D() < VEL_EPSILON) ?
			CCircuitDef::ThreatType::LAND : CCircuitDef::ThreatType::VEL_LAND);
	const int rangeLandSq = SQUARE(rangeLand);
	const int rangeWater = CEnemyUnit::GetRange(e.range, (e.vel.SqLength2D() < VEL_EPSILON) ?
			CCircuitDef::ThreatType::WATER : CCircuitDef::ThreatType::VEL_WATER);
	const int rangeWaterSq = SQUARE(rangeWater);
	const int range = std::max(rangeLand, rangeWater);
	const std::vector<STerrainMapSector>& sector = areaData->sector;

	const int beginX = std::max(int(posx - range + 1),      0);
	const int endX   = std::min(int(posx + range    ),  width);
	const int beginZ = std::max(int(posz - range + 1),      0);
	const int endZ   = std::min(int(posz + range    ), height);

	for (int z = beginZ; z < endZ; ++z) {
		const int dzSq = SQUARE(posz - z);
		for (int x = beginX; x < endX; ++x) {
			const int dxSq = SQUARE(posx - x);

			const int sum = dxSq + dzSq;
			const int index = z * width + x;
			bool isWaterThreat = (sum <= rangeWaterSq) && sector[index].isWater;
			if (isWaterThreat || ((sum <= rangeLandSq) && (sector[index].position.y >= -SQUARE_SIZE * 5)))
			{
				drawAmphThreat[index] += threat;
			}
			if (isWaterThreat || (sum <= rangeLandSq))
			{
				drawSurfThreat[index] += threat;
			}
		}
	}
}

void CThreatMap::AddEnemyAmphGradient(const CEnemyUnit::SData& e)
{
	int posx, posz;
	PosToXZ(e.pos, posx, posz);

	const float threat = e.threat/* - THREAT_DECAY*/;
	const int rangeLand = CEnemyUnit::GetRange(e.range, (e.vel.SqLength2D() < VEL_EPSILON) ?
			CCircuitDef::ThreatType::LAND : CCircuitDef::ThreatType::VEL_LAND);
	const int rangeLandSq = SQUARE(rangeLand);
	const int rangeWater = CEnemyUnit::GetRange(e.range, (e.vel.SqLength2D() < VEL_EPSILON) ?
			CCircuitDef::ThreatType::WATER : CCircuitDef::ThreatType::VEL_WATER);
	const int rangeWaterSq = SQUARE(rangeWater);
	const int range = std::max(rangeLand, rangeWater);
	const std::vector<STerrainMapSector>& sector = areaData->sector;

	const int beginX = std::max(int(posx - range + 1),      0);
	const int endX   = std::min(int(posx + range    ),  width);
	const int beginZ = std::max(int(posz - range + 1),      0);
	const int endZ   = std::min(int(posz + range    ), height);

	for (int z = beginZ; z < endZ; ++z) {
		const int dzSq = SQUARE(posz - z);
		for (int x = beginX; x < endX; ++x) {
			const int dxSq = SQUARE(posx - x);

			// TODO: 1) Draw as LOS. 2) Separate draw rules for artillery, superweapons, instant-hit weapons
			// Arty: center have no/little threat
			// Super: same as arty, or totally ignore its threat
			// Laser: no gradient
			const int sum = dxSq + dzSq;
			const int index = z * width + x;
			const float heat = threat * (2.0f - 1.0f * sqrtf(sum) / range);
			bool isWaterThreat = (sum <= rangeWaterSq) && sector[index].isWater;
			if (isWaterThreat || ((sum <= rangeLandSq) && (sector[index].position.y >= -SQUARE_SIZE * 5)))
			{
				drawAmphThreat[index] += heat;
			}
			if (isWaterThreat || (sum <= rangeLandSq))
			{
				drawSurfThreat[index] += heat;
			}
		}
	}
}

void CThreatMap::AddDecloaker(const CEnemyUnit::SData& e)
{
	int posx, posz;
	PosToXZ(e.pos, posx, posz);

	const float threatCloak = THREAT_CLOAK;
	const int rangeCloak = CEnemyUnit::GetRange(e.range, CCircuitDef::ThreatType::CLOAK);
	const int rangeCloakSq = SQUARE(rangeCloak);

	// For small decloak ranges full range shouldn't hit performance
	const int beginX = std::max(int(posx - rangeCloak + 1),      0);
	const int endX   = std::min(int(posx + rangeCloak    ),  width);
	const int beginZ = std::max(int(posz - rangeCloak + 1),      0);
	const int endZ   = std::min(int(posz + rangeCloak    ), height);

	for (int z = beginZ; z < endZ; ++z) {
		const int dzSq = SQUARE(posz - z);
		for (int x = beginX; x < endX; ++x) {
			const int dxSq = SQUARE(posx - x);
			const int sum = dxSq + dzSq;
			if (sum > rangeCloakSq) {
				continue;
			}

			const int index = z * width + x;
			const float heat = threatCloak * (1.0f - 0.75f * sqrtf(sum) / rangeCloak);
			drawCloakThreat[index] += heat;
		}
	}
}

void CThreatMap::AddShield(const CEnemyUnit::SData& e)
{
	int posx, posz;
	PosToXZ(e.pos, posx, posz);

	const float shieldVal = e.shieldPower;
	const int rangeShield = CEnemyUnit::GetRange(e.range, CCircuitDef::ThreatType::SHIELD);
	const int rangeShieldSq = SQUARE(rangeShield);

	const int beginX = std::max(int(posx - rangeShield + 1),      0);
	const int endX   = std::min(int(posx + rangeShield    ),  width);
	const int beginZ = std::max(int(posz - rangeShield + 1),      0);
	const int endZ   = std::min(int(posz + rangeShield    ), height);

	for (int z = beginZ; z < endZ; ++z) {
		const int rrz = rangeShieldSq - SQUARE(posz - z);
		for (int x = beginX; x < endX; ++x) {
			if (SQUARE(posx - x) > rrz) {
				continue;
			}
			drawShieldArray[z * width + x] += shieldVal;
		}
	}
}

void CThreatMap::SetEnemyUnitRange(CEnemyUnit* e) const
{
	const CCircuitDef* edef = e->GetCircuitDef();
	assert(edef != nullptr);

	// FIXME: DEBUG  comm's threat value is not based on proper weapons
	if (edef->IsRoleComm()) {
		// TODO: by weapons 1,2 descriptions set proper land/air/water ranges/threats
		float maxRange = 0.f;
		float maxAoe = 0.f;
		for (int num = 1; num < 3; ++num) {
			std::string str = utils::int_to_string(num, "comm_weapon_id_%i");
			int weaponDefId = int(e->GetUnit()->GetRulesParamFloat(str.c_str(), -1));
			if (weaponDefId < 0) {
				continue;
			}

			CWeaponDef* weaponDef = circuit->GetWeaponDef(weaponDefId);
			const float range = weaponDef->GetRange();
			if (maxRange < range) {
				maxRange = range;
				maxAoe = weaponDef->GetAoe();
			}
		}
		const float velSlack = DEFAULT_SLACK * slackMod + edef->GetSpeed() / FRAMES_PER_SEC * THREAT_UPDATE_RATE;
		const float slack = squareSize - 1 +  maxAoe;
		const float mult = e->GetUnit()->GetRulesParamFloat("comm_range_mult", 1.f);
		const float range = int(maxRange * mult + slack) / squareSize + 1;
		const float velRange = int(maxRange * mult + slack + velSlack) / squareSize + 1;
		e->SetRange(CCircuitDef::ThreatType::MAX, range);
		e->SetRange(CCircuitDef::ThreatType::AIR, range);
		e->SetRange(CCircuitDef::ThreatType::LAND, range);
		e->SetRange(CCircuitDef::ThreatType::WATER, range);
		e->SetRange(CCircuitDef::ThreatType::VEL_AIR, velRange);
		e->SetRange(CCircuitDef::ThreatType::VEL_LAND, velRange);
		e->SetRange(CCircuitDef::ThreatType::VEL_WATER, velRange);
		e->SetRange(CCircuitDef::ThreatType::CLOAK, edef->GetThreatRange(CCircuitDef::ThreatType::CLOAK));
		e->SetRange(CCircuitDef::ThreatType::SHIELD, edef->GetThreatRange(CCircuitDef::ThreatType::SHIELD));
	} else {
	// FIXME: DEBUG

		for (CCircuitDef::ThreatT tt = 0; tt < static_cast<CCircuitDef::ThreatT>(CCircuitDef::ThreatType::_SIZE_); ++tt) {
			CCircuitDef::ThreatType type = static_cast<CCircuitDef::ThreatType>(tt);
			e->SetRange(type, edef->GetThreatRange(type));
		}
	}
}

int CThreatMap::GetCloakRange(const CCircuitDef* edef) const
{
	const int sizeX = edef->GetDef()->GetXSize() * (SQUARE_SIZE / 2);
	const int sizeZ = edef->GetDef()->GetZSize() * (SQUARE_SIZE / 2);
	int threatRange = distCloak;
	if (edef->IsMobile()) {
		threatRange += (DEFAULT_SLACK * 2) / squareSize;
	}
	return (int)sqrtf(SQUARE(sizeX) + SQUARE(sizeZ)) / squareSize + threatRange;
}

int CThreatMap::GetShieldRange(const CCircuitDef* edef) const
{
	return (edef->GetShieldMount() != nullptr) ? (int)edef->GetShieldRadius() / squareSize + 1 : 0;
}

float CThreatMap::GetEnemyUnitThreat(CEnemyUnit* enemy) const
{
	if (enemy->IsBeingBuilt()) {
		return .0f;  // THREAT_BASE;
	}
	const float health = enemy->GetHealth();
	if (health <= .0f) {
		return .0f;
	}
	int x, z;
	PosToXZ(enemy->GetPos(), x, z);
	return enemy->GetDamage() * sqrtf(health + shieldArray[z * width + x] * 2.0f);  // / unit->GetUnit()->GetMaxHealth();
}

bool CThreatMap::IsInLOS(const AIFloat3& pos) const
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

//bool CThreatMap::IsInRadar(const AIFloat3& pos) const
//{
//	// the value for the full resolution position (x, z) is at index ((z * width + x) / res)
//	// the last value, bottom right, is at index (width/res * height/res - 1)
//
//	// convert from world coordinates to radarmap coordinates
//	const int x = (int)pos.x / radarResConv;
//	const int z = (int)pos.z / radarResConv;
//	return ((pos.y < -SQUARE_SIZE * 5) ? sonarMap : radarMap)[z * radarWidth + x] > 0;
//}

void CThreatMap::Update()
{
	Prepare(*GetNextThreatData());

	for (const CEnemyUnit::SData& e : hostileDatas) {
		AddEnemyUnit(e);
	}

	for (const CEnemyUnit::SData& e : peaceDatas) {
		AddDecloaker(e);
	}

	hostileDatas.clear();
	peaceDatas.clear();
}

void CThreatMap::Apply()
{
	pThreatData = GetNextThreatData();
	isUpdating = false;

#ifdef DEBUG_VIS
	UpdateVis();
#endif
}

#ifdef DEBUG_VIS
void CThreatMap::UpdateVis()
{
	if (isWidgetDrawing || isWidgetPrinting) {
		std::ostringstream cmd;
		cmd << "ai_thr_data:";
		for (int i = 0; i < mapSize; ++i) {
			cmd << surfThreat[i] << " ";
		}
		std::string s = cmd.str();
		circuit->GetLua()->CallRules(s.c_str(), s.size());
	}

	if (sdlWindows.empty()/* || (currMaxThreat < .1f)*/) {
		return;
	}

	Uint32 sdlWindowId;
	float* dbgMap;
	std::tie(sdlWindowId, dbgMap) = sdlWindows[0];
	for (int i = 0; i < mapSize; ++i) {
		dbgMap[i] = std::min<float>((airThreat[i] - THREAT_BASE) / 40.0f /*currMaxThreat*/, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap);

	std::tie(sdlWindowId, dbgMap) = sdlWindows[1];
	for (int i = 0; i < mapSize; ++i) {
		dbgMap[i] = std::min<float>((surfThreat[i] - THREAT_BASE) / 40.0f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap);

	std::tie(sdlWindowId, dbgMap) = sdlWindows[2];
	for (int i = 0; i < mapSize; ++i) {
		dbgMap[i] = std::min<float>((amphThreat[i] - THREAT_BASE) / 40.0f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap);

	std::tie(sdlWindowId, dbgMap) = sdlWindows[3];
	for (int i = 0; i < mapSize; ++i) {
		dbgMap[i] = std::min<float>((cloakThreat[i] - THREAT_BASE) / THREAT_CLOAK, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap);
}

void CThreatMap::ToggleSDLVis()
{
	if (sdlWindows.empty()) {
		// ~threat
		std::pair<Uint32, float*> win;
		std::string label;

		win.second = new float [mapSize];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: AIR Threat Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [mapSize];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: SURFACE Threat Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [mapSize];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: AMPHIBIOUS Threat Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [mapSize];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: CLOAK Threat Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		UpdateVis();
	} else {
		for (const std::pair<Uint32, float*>& win : sdlWindows) {
			circuit->GetDebugDrawer()->DelSDLWindow(win.first);
			delete[] win.second;
		}
		sdlWindows.clear();
	}
}

void CThreatMap::ToggleWidgetDraw()
{
	std::string cmd("ai_thr_draw:");
	std::string result = circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

	isWidgetDrawing = (result == "1");
	if (isWidgetDrawing) {
		cmd = utils::int_to_string(squareSize, "ai_thr_size:%i");
		cmd += utils::float_to_string(THREAT_BASE, " %f");
		circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

		UpdateVis();
	}
}

void CThreatMap::ToggleWidgetPrint()
{
	std::string cmd("ai_thr_print:");
	std::string result = circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

	isWidgetPrinting = (result == "1");
	if (isWidgetPrinting) {
		cmd = utils::int_to_string(squareSize, "ai_thr_size:%i");
		cmd += utils::float_to_string(THREAT_BASE, " %f");
		circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

		UpdateVis();
	}
}

void CThreatMap::SetMaxThreat(float maxThreat)
{
	std::string cmd = utils::float_to_string(maxThreat, "ai_thr_div:%f");
	circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());
}
#endif

} // namespace circuit
