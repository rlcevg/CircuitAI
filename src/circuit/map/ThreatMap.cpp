/*
 * ThreatMap.cpp
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 */

#include "map/ThreatMap.h"
#include "map/MapManager.h"
#include "scheduler/Scheduler.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"
#include "json/json.h"

#ifdef DEBUG_VIS
#include "Lua.h"
#endif

//#undef NDEBUG
#include <cassert>

namespace circuit {

using namespace springai;

#define THREAT_DECAY	1e-2f
#define THREAT_CLOAK	16.0f
#define VEL_EPSILON		1e-2f

CThreatMap::CThreatMap(CMapManager* manager, float decloakRadius)
		: manager(manager)
		, pThreatData(&threatData0)
		, isUpdating(false)
{
	CCircuitAI* circuit = manager->GetCircuit();
	areaData = circuit->GetTerrainManager()->GetAreaData();
	squareSize = circuit->GetTerrainManager()->GetConvertStoP();
	width = circuit->GetTerrainManager()->GetSectorXSize();
	height = circuit->GetTerrainManager()->GetSectorZSize();
	mapSize = width * height;

	rangeDefault = (DEFAULT_SLACK * 4) / squareSize;
	distCloak = (decloakRadius + DEFAULT_SLACK) / squareSize;

	threatData0.roleThreats.resize(1/*CMaskHandler::GetMaxMasks()*/);
	for (SRoleThreat& roleThreat : threatData0.roleThreats) {
		roleThreat.airThreat.resize(mapSize, THREAT_BASE);
		roleThreat.surfThreat.resize(mapSize, THREAT_BASE);
		roleThreat.amphThreat.resize(mapSize, THREAT_BASE);
	}
	threatData0.cloakThreat.resize(mapSize, THREAT_BASE);
	threatData0.shield.resize(mapSize, 0.f);
	airThreat = threatData0.roleThreats[0].airThreat.data();
	surfThreat = threatData0.roleThreats[0].surfThreat.data();
	amphThreat = threatData0.roleThreats[0].amphThreat.data();
	cloakThreat = threatData0.cloakThreat.data();
	shieldArray = threatData0.shield.data();
	threatArray = surfThreat;

	threatData1.roleThreats.resize(1/*CMaskHandler::GetMaxMasks()*/);
	for (SRoleThreat& roleThreat : threatData1.roleThreats) {
		roleThreat.airThreat.resize(mapSize, THREAT_BASE);
		roleThreat.surfThreat.resize(mapSize, THREAT_BASE);
		roleThreat.amphThreat.resize(mapSize, THREAT_BASE);
	}
	threatData1.cloakThreat.resize(mapSize, THREAT_BASE);
	threatData1.shield.resize(mapSize, 0.f);
	drawCloakThreat = threatData1.cloakThreat.data();
	drawShieldArray = threatData1.shield.data();

	const Json::Value& slack = circuit->GetSetupManager()->GetConfig()["quota"]["slack_mod"];
	slackMod.allMod = slack.get("all", 1.f).asFloat();
	slackMod.staticMod = slack.get("static", 1.f).asFloat();
	const Json::Value& speedSlack = slack["speed"];
	slackMod.speedMod = speedSlack.get((unsigned)0, 1.f).asFloat() * DEFAULT_SLACK / squareSize;
	slackMod.speedModMax = speedSlack.get((unsigned)1, 2).asInt() * DEFAULT_SLACK / squareSize;
	constexpr float allowedRange = 2000.f;
	constexpr float allowedSpeed = 200.f;
	for (CCircuitDef& cdef : circuit->GetCircuitDefs()) {
		float speed = cdef.GetSpeed();
		float slack = squareSize - 1 + cdef.GetAoe() / 2 + DEFAULT_SLACK * slackMod.allMod;
		if (cdef.IsMobile()) {
			// TODO: slack should also depend on own unit's path update rate
			slack += THREAT_UPDATE_RATE * speed / FRAMES_PER_SEC;
		} else {
			slack += DEFAULT_SLACK * slackMod.staticMod;
		}
		float realRange;
		int range;

		realRange = cdef.GetMaxRange(CCircuitDef::RangeType::AIR);
		range = cdef.HasSurfToAir() ? int(realRange + slack) / squareSize + 1 : 0;
		cdef.SetThreatRange(CCircuitDef::ThreatType::AIR, range);

		realRange = cdef.GetMaxRange(CCircuitDef::RangeType::LAND);
		range = (cdef.HasSurfToLand() && (realRange <= allowedRange) && (speed <= allowedSpeed)) ? int(realRange + slack) / squareSize + 1 : 0;
		cdef.SetThreatRange(CCircuitDef::ThreatType::LAND, range);

		realRange = cdef.GetMaxRange(CCircuitDef::RangeType::WATER);
		range = (cdef.HasSurfToWater() && (realRange <= allowedRange) && (speed <= allowedSpeed)) ? int(realRange + slack) / squareSize + 1 : 0;
		cdef.SetThreatRange(CCircuitDef::ThreatType::WATER, range);

		cdef.SetThreatRange(CCircuitDef::ThreatType::CLOAK, GetCloakRange(&cdef));
		cdef.SetThreatRange(CCircuitDef::ThreatType::SHIELD, GetShieldRange(&cdef));
	}
}

CThreatMap::~CThreatMap()
{
#ifdef DEBUG_VIS
	CCircuitAI* circuit = manager->GetCircuit();
	for (const std::pair<Uint32, float*>& win : sdlWindows) {
		circuit->GetDebugDrawer()->DelSDLWindow(win.first);
		delete[] win.second;
	}
#endif
}

void CThreatMap::EnqueueUpdate()
{
//	if (isUpdating) {
//		return;
//	}
	isUpdating = true;

	CCircuitAI* circuit = manager->GetCircuit();
	areaData = circuit->GetTerrainManager()->GetAreaData();

	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunPriorityJob(CScheduler::WorkJob(&CThreatMap::Update, this, scheduler));
}

void CThreatMap::SetEnemyUnitRange(CEnemyUnit* e) const
{
	const CCircuitDef* edef = e->GetCircuitDef();
	assert(edef != nullptr);

	// FIXME: DEBUG  comm's threat value is not based on proper weapons
//	if (edef->IsRoleComm() && !e->IsFake()) {
//		CCircuitAI* circuit = manager->GetCircuit();
//		// TODO: by weapons 1,2 descriptions set proper land/air/water ranges/threats
//		float maxRange = 0.f;
//		float maxAoe = 0.f;
//		for (int num = 1; num < 3; ++num) {
//			std::string str = utils::int_to_string(num, "comm_weapon_id_%i");
//			int weaponDefId = int(e->GetUnit()->GetRulesParamFloat(str.c_str(), -1));
//			if (weaponDefId < 0) {
//				continue;
//			}
//
//			CWeaponDef* weaponDef = circuit->GetWeaponDef(weaponDefId);
//			const float range = weaponDef->GetRange();
//			if (maxRange < range) {
//				maxRange = range;
//				maxAoe = weaponDef->GetAoe();
//			}
//		}
//		const float slack = squareSize - 1 + maxAoe / 2
//				+ DEFAULT_SLACK * slackMod.allMod
//				+ THREAT_UPDATE_RATE * edef->GetSpeed() / FRAMES_PER_SEC;
//		const float mult = e->GetUnit()->GetRulesParamFloat("comm_range_mult", 1.f);
//		const float range = int(maxRange * mult + slack) / squareSize + 1;
//		e->SetRange(CCircuitDef::ThreatType::AIR, range);
//		e->SetRange(CCircuitDef::ThreatType::LAND, range);
//		e->SetRange(CCircuitDef::ThreatType::WATER, range);
//		e->SetRange(CCircuitDef::ThreatType::CLOAK, edef->GetThreatRange(CCircuitDef::ThreatType::CLOAK));
//		e->SetRange(CCircuitDef::ThreatType::SHIELD, edef->GetThreatRange(CCircuitDef::ThreatType::SHIELD));
//	} else {
	// FIXME: DEBUG

		for (CCircuitDef::ThreatT tt = 0; tt < static_cast<CCircuitDef::ThreatT>(CCircuitDef::ThreatType::_SIZE_); ++tt) {
			CCircuitDef::ThreatType type = static_cast<CCircuitDef::ThreatType>(tt);
			e->SetRange(type, edef->GetThreatRange(type));
		}
//	}
}

void CThreatMap::NewEnemy(CEnemyUnit* e) const
{
	e->SetThreat(e->GetDamage());  // TODO: Randomize
	e->SetRange(CCircuitDef::ThreatType::AIR, rangeDefault);
	e->SetRange(CCircuitDef::ThreatType::LAND, rangeDefault);
	e->SetRange(CCircuitDef::ThreatType::WATER, rangeDefault);
	e->SetRange(CCircuitDef::ThreatType::CLOAK, distCloak);
//	e->SetRange(CCircuitDef::ThreatType::SHIELD, 0);
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
	float health = unit->GetUnit()->GetHealth() + unit->GetShieldPower() * SHIELD_MOD;
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

void CThreatMap::AddEnemyUnit(const SEnemyData& e)
{
	CCircuitDef* cdef = e.cdef;
	if (cdef == nullptr) {
		AddEnemyUnitAll(e);
		return;
	}

	if (cdef->IsInWater(areaData->GetElevationAt(e.pos.x, e.pos.z), e.pos.y)) {
		if (cdef->HasSubToAir()) {
			airDraws.push_back(&e);
		}
		if (cdef->HasSubToLand() || cdef->HasSubToWater()) {
			amphDraws.push_back(&e);
		}
	} else {
		if (cdef->HasSurfToAir()) {
			airDraws.push_back(&e);
		}
		if (cdef->HasSurfToLand() || cdef->HasSurfToWater()) {
			amphDraws.push_back(&e);
		}
	}
	cloakDraws.push_back(&e);

	if (cdef->GetShieldMount() != nullptr) {
		shieldDraws.push_back(&e);
	}
}

void CThreatMap::AddEnemyUnitAll(const SEnemyData& e)
{
	airDraws.push_back(&e);
	amphDraws.push_back(&e);
	cloakDraws.push_back(&e);
}

void CThreatMap::AddEnemyAir(float* drawAirThreat, const SEnemyData& e, const int slack)
{
	int posx, posz;
	PosToXZ(e.pos, posx, posz);

	const float threat = e.threat/* - THREAT_DECAY*/;
	const int range = e.GetRange(CCircuitDef::ThreatType::AIR) + slack;
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
			const float heat = threat * (1.0f - 0.5f * sqrtf(sum) / range);
			// MicroPather cannot deal with negative costs
			// (which may arise due to floating-point drift)
			// nor with zero-cost nodes (see MP::SetMapData,
			// threat is not used as an additive overlay)
			drawAirThreat[index] += heat;
		}
	}
}

void CThreatMap::AddEnemyAmphConst(float* drawSurfThreat, float* drawAmphThreat, const SEnemyData& e, const int slack)
{
	int posx, posz;
	PosToXZ(e.pos, posx, posz);

	const float threat = e.threat/* - THREAT_DECAY*/;
	int r = e.GetRange(CCircuitDef::ThreatType::LAND);
	const int rangeLand = (r > 0) ? r + slack : 0;
	const int rangeLandSq = (r > 0) ? SQUARE(rangeLand) : -1;
	r = e.GetRange(CCircuitDef::ThreatType::WATER);
	const int rangeWater = (r > 0) ? r + slack : 0;
	const int rangeWaterSq = (r > 0) ? SQUARE(rangeWater) : -1;
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

void CThreatMap::AddEnemyAmphGradient(float* drawSurfThreat, float* drawAmphThreat, const SEnemyData& e, const int slack)
{
	int posx, posz;
	PosToXZ(e.pos, posx, posz);

	const float threat = e.threat/* - THREAT_DECAY*/;
	int r = e.GetRange(CCircuitDef::ThreatType::LAND);
	const int rangeLand = (r > 0) ? r + slack : 0;
	const int rangeLandSq = (r > 0) ? SQUARE(rangeLand) : -1;
	r = e.GetRange(CCircuitDef::ThreatType::WATER);
	const int rangeWater = (r > 0) ? r + slack : 0;
	const int rangeWaterSq = (r > 0) ? SQUARE(rangeWater) : -1;
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
			const float heat = threat * (1.0f - 0.5f * sqrtf(sum) / range);
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

void CThreatMap::AddDecloaker(const SEnemyData& e)
{
	int posx, posz;
	PosToXZ(e.pos, posx, posz);

	const float threatCloak = THREAT_CLOAK;
	const int rangeCloak = e.GetRange(CCircuitDef::ThreatType::CLOAK);
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

void CThreatMap::AddShield(const SEnemyData& e)
{
	int posx, posz;
	PosToXZ(e.pos, posx, posz);

	const float shieldVal = e.shieldPower;
	const int rangeShield = e.GetRange(CCircuitDef::ThreatType::SHIELD);
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

float CThreatMap::GetEnemyUnitThreat(const CEnemyUnit* e) const
{
//	if (e->IsBeingBuilt()) {
//		return .0f;  // THREAT_BASE;
//	}
	const float health = e->GetHealth();
	if (health <= .0f) {
		return .0f;
	}
	int x, z;
	PosToXZ(e->GetPos(), x, z);
	return e->GetDamage() * sqrtf(health + shieldArray[z * width + x] * SHIELD_MOD);  // / unit->GetUnit()->GetMaxHealth();
}

std::shared_ptr<IMainJob> CThreatMap::AirDrawer(int roleNum)
{
	SThreatData& threatData = *GetNextThreatData();
	SRoleThreat& roleThreat = threatData.roleThreats[roleNum];
	std::fill(roleThreat.airThreat.begin(), roleThreat.airThreat.end(), THREAT_BASE);

	for (const SEnemyData* e : airDraws) {
		if (e->cdef == nullptr) {
			AddEnemyAir(roleThreat.airThreat.data(), *e);
		} else {
			const int vsl = std::min(int(e->vel.Length2D() * slackMod.speedMod), slackMod.speedModMax);
			AddEnemyAir(roleThreat.airThreat.data(), *e, vsl);
		}
	}
	return ApplyDrawers();
}

std::shared_ptr<IMainJob> CThreatMap::AmphDrawer(int roleNum)
{
	SThreatData& threatData = *GetNextThreatData();
	SRoleThreat& roleThreat = threatData.roleThreats[roleNum];
	std::fill(roleThreat.surfThreat.begin(), roleThreat.surfThreat.end(), THREAT_BASE);
	std::fill(roleThreat.amphThreat.begin(), roleThreat.amphThreat.end(), THREAT_BASE);
	float* drawSurfThreat = roleThreat.surfThreat.data();
	float* drawAmphThreat = roleThreat.amphThreat.data();

	for (const SEnemyData* e : amphDraws) {
		if (e->cdef == nullptr) {
			AddEnemyAmphGradient(drawSurfThreat, drawAmphThreat, *e);
		} else {
			const int vsl = std::min(int(e->vel.Length2D() * slackMod.speedMod), slackMod.speedModMax);
			e->cdef->IsAlwaysHit() ? AddEnemyAmphConst(drawSurfThreat, drawAmphThreat, *e, vsl) : AddEnemyAmphGradient(drawSurfThreat, drawAmphThreat, *e, vsl);
		}
	}
	return ApplyDrawers();
}

std::shared_ptr<IMainJob> CThreatMap::CloakDrawer()
{
	for (const SEnemyData* e : cloakDraws) {
		AddDecloaker(*e);
	}
	cloakDraws.clear();
	return ApplyDrawers();
}

std::shared_ptr<IMainJob> CThreatMap::ShieldDrawer()
{
	for (const SEnemyData* e : shieldDraws) {
		AddShield(*e);
	}
	shieldDraws.clear();
	return ApplyDrawers();
}

std::shared_ptr<IMainJob> CThreatMap::ApplyDrawers()
{
//	return (--numThreadDraws == 0) ? CScheduler::GameJob(&CThreatMap::Apply, this) : nullptr;
	// FIXME: DEBUG
	if (--numThreadDraws == 0) {
//		int count = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - t0).count();
//		printf("%i | %i mcs\n", manager->GetCircuit()->GetSkirmishAIId(), count);
		return CScheduler::GameJob(&CThreatMap::Apply, this);
	} else {
		return nullptr;
	}
	// FIXME: DEBUG
}

void CThreatMap::Prepare(SThreatData& threatData)
{
	std::fill(threatData.cloakThreat.begin(), threatData.cloakThreat.end(), THREAT_BASE);
	std::fill(threatData.shield.begin(), threatData.shield.end(), 0.f);

	drawCloakThreat = threatData.cloakThreat.data();
	drawShieldArray = threatData.shield.data();

	airDraws.clear();
	amphDraws.clear();
}

std::shared_ptr<IMainJob> CThreatMap::Update(CScheduler* scheduler)
{
	// FIXME: DEBUG
//	t0 = clock::now();
	// FIXME: DEBUG
	SThreatData& threatData = *GetNextThreatData();
	Prepare(threatData);

	CEnemyManager* enemyMgr = manager->GetCircuit()->GetEnemyManager();

	for (const SEnemyData& e : enemyMgr->GetHostileDatas()) {
		AddEnemyUnit(e);
	}

	for (const SEnemyData& e : enemyMgr->GetPeaceDatas()) {
		cloakDraws.push_back(&e);
	}

	numThreadDraws = 2 + 2 * threatData.roleThreats.size();
	for (unsigned int i = 0; i < threatData.roleThreats.size(); ++i) {
		scheduler->RunPriorityJob(CScheduler::WorkJob(&CThreatMap::AirDrawer, this, i));
		scheduler->RunPriorityJob(CScheduler::WorkJob(&CThreatMap::AmphDrawer, this, i));
	}
	scheduler->RunPriorityJob(CScheduler::WorkJob(&CThreatMap::CloakDrawer, this));
	scheduler->RunPriorityJob(CScheduler::WorkJob(&CThreatMap::ShieldDrawer, this));

	return nullptr;
}

void CThreatMap::Apply()
{
	if (!isUpdating) {
		return;
	}

	SwapBuffers();
	isUpdating = false;

#ifdef DEBUG_VIS
	UpdateVis();
#endif
}

void CThreatMap::SwapBuffers()
{
	pThreatData = GetNextThreatData();
	SThreatData& threatData = *pThreatData.load();
	airThreat = threatData.roleThreats[0].airThreat.data();
	surfThreat = threatData.roleThreats[0].surfThreat.data();
	amphThreat = threatData.roleThreats[0].amphThreat.data();
	cloakThreat = threatData.cloakThreat.data();
	shieldArray = threatData.shield.data();
	threatArray = surfThreat;
}

#ifdef DEBUG_VIS
void CThreatMap::UpdateVis()
{
	CCircuitAI* circuit = manager->GetCircuit();
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
	CCircuitAI* circuit = manager->GetCircuit();
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
	CCircuitAI* circuit = manager->GetCircuit();
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
	CCircuitAI* circuit = manager->GetCircuit();
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
	CCircuitAI* circuit = manager->GetCircuit();
	std::string cmd = utils::float_to_string(maxThreat, "ai_thr_div:%f");
	circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());
}
#endif

} // namespace circuit
