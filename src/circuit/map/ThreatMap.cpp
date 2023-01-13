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

//#undef NDEBUG
#include <cassert>

namespace circuit {

using namespace springai;
using namespace terrain;

#define THREAT_DECAY	1e-2f
#define THREAT_CLOAK	16.0f
#define VEL_EPSILON		1e-2f

CThreatMap::CThreatMap(CMapManager* manager, float decloakRadius)
		: manager(manager)
		, pThreatData(&threatData0)
		, defRole(ROLE_TYPE(BUILDER))
		, isUpdating(false)
		, cloakThreat(nullptr)
		, shieldArray(nullptr)
		, threatArray(nullptr)
{
	CCircuitAI* circuit = manager->GetCircuit();
	areaData = circuit->GetTerrainManager()->GetAreaData();
	squareSize = circuit->GetTerrainManager()->GetConvertStoP();
	width = circuit->GetTerrainManager()->GetSectorXSize();
	height = circuit->GetTerrainManager()->GetSectorZSize();
	mapSize = width * height;

	rangeDefault = (DEFAULT_SLACK * 4) / squareSize;
	distCloak = (decloakRadius + DEFAULT_SLACK) / squareSize;

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
		cdef.SetThreatRange(CCircuitDef::ThreatType::SURF, range);

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

void CThreatMap::Init(const int roleSize, std::set<CCircuitDef::RoleT>&& modRoles)
{
	defRole = 0;
	if ((int)modRoles.size() < roleSize) {
		for (CCircuitDef::RoleT role : modRoles) {
			if (role != defRole) {
				break;
			}
			++defRole;
		}
		modRoles.insert(defRole);
	}

	for (CCircuitDef::RoleT role : modRoles) {
		for (SThreatData* threatData : {&threatData0, &threatData1}) {
			SRoleThreat& roleThreat = threatData->roleThreats[role];
			roleThreat.airThreat.resize(mapSize, THREAT_BASE);
			roleThreat.surfThreat.resize(mapSize, THREAT_BASE);
			roleThreat.amphThreat.resize(mapSize, THREAT_BASE);
			roleThreat.swimThreat.resize(mapSize, THREAT_BASE);
		}
	}

	threatData0.defThreat = &threatData0.roleThreats[defRole];
	threatData1.defThreat = &threatData1.roleThreats[defRole];

	threatData0.roleThreatPtrs.resize(roleSize);
	threatData1.roleThreatPtrs.resize(roleSize);
	for (CCircuitDef::RoleT i = 0; i < roleSize; ++i) {
		if (modRoles.find(i) == modRoles.end()) {
			threatData0.roleThreatPtrs[i] = threatData0.defThreat;
			threatData1.roleThreatPtrs[i] = threatData1.defThreat;
		} else {
			threatData0.roleThreatPtrs[i] = &threatData0.roleThreats[i];
			threatData1.roleThreatPtrs[i] = &threatData1.roleThreats[i];
		}
	}
	threatData0.cloakThreat.resize(mapSize, THREAT_BASE);
	threatData1.cloakThreat.resize(mapSize, THREAT_BASE);
	threatData0.shield.resize(mapSize, 0.f);
	threatData1.shield.resize(mapSize, 0.f);

	cloakThreat = threatData0.cloakThreat.data();
	shieldArray = threatData0.shield.data();
	threatArray = threatData0.defThreat->surfThreat.data();
}

void CThreatMap::CopyDefs(CCircuitAI* ally)
{
	CCircuitAI* circuit = manager->GetCircuit();
	for (CCircuitDef& cdef : ally->GetCircuitDefs()) {
		for (CCircuitDef::ThreatT tt = 0; tt < static_cast<CCircuitDef::ThreatT>(CCircuitDef::ThreatType::_SIZE_); ++tt) {
			CCircuitDef::ThreatType type = static_cast<CCircuitDef::ThreatType>(tt);
			cdef.SetThreatRange(type, circuit->GetCircuitDef(cdef.GetId())->GetThreatRange(type));
		}
	}
}

void CThreatMap::EnqueueUpdate()
{
//	if (isUpdating) {
//		return;
//	}
	isUpdating = true;

	CCircuitAI* circuit = manager->GetCircuit();
	areaData = circuit->GetTerrainManager()->GetAreaData();

	CEnemyManager* enemyMgr = circuit->GetEnemyManager();
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunPriorityJob(CScheduler::WorkJob(&CThreatMap::Update, this, enemyMgr, scheduler));
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

void CThreatMap::SetEnemyUnitThreat(CEnemyUnit* e) const
{
	const float health = GetThreatHealth(e);
	if (health > 0.f) {
		int x, z;
		PosToXZ(e->GetPos(), x, z);
		const float healthMod = sqrtf(health + shieldArray[z * width + x] * SHIELD_MOD);  // / unit->GetUnit()->GetMaxHealth();
		e->SetThrHealth(healthMod);
		e->SetInfluence(e->GetDefDamage() * healthMod);
	} else {
		e->ClearThreat();
	}
}

void CThreatMap::NewEnemy(CEnemyUnit* e) const
{
//	e->SetThreatMod(1.f);  // e->GetDamage()
	e->SetInfluence(e->GetDefDamage());
	e->SetRange(CCircuitDef::ThreatType::AIR, rangeDefault);
	e->SetRange(CCircuitDef::ThreatType::SURF, rangeDefault);
	e->SetRange(CCircuitDef::ThreatType::WATER, rangeDefault);
	e->SetRange(CCircuitDef::ThreatType::CLOAK, distCloak);
//	e->SetRange(CCircuitDef::ThreatType::SHIELD, 0);
}

float CThreatMap::GetBuilderThreatAt(const AIFloat3& position) const
{
	assert((position.x >= 0) && (position.x < CTerrainManager::GetTerrainWidth()) &&
		   (position.z >= 0) && (position.z < CTerrainManager::GetTerrainHeight()));
	int x, z;
	PosToXZ(position, x, z);
	const int index = z * width + x;
//	float air = airThreat[index] - THREAT_BASE;
	float land = pThreatData.load()->roleThreatPtrs[ROLE_TYPE(BUILDER)]->surfThreat[index] - THREAT_BASE;
//	float water = amphThreat[index] - THREAT_BASE;
	return /*air + */land/* + water*/;
}

void CThreatMap::SetThreatType(CCircuitUnit* unit)
{
	assert(unit != nullptr);
	CCircuitDef::RoleT role = unit->GetCircuitDef()->GetMainRole();
	if (unit->GetCircuitDef()->IsAbleToFly()) {
		threatArray = pThreatData.load()->roleThreatPtrs[role]->airThreat.data();
	} else if (unit->GetCircuitDef()->IsAbleToDive()) {
		threatArray = pThreatData.load()->roleThreatPtrs[role]->amphThreat.data();
	} else if (unit->GetCircuitDef()->IsAbleToSwim()) {
		threatArray = pThreatData.load()->roleThreatPtrs[role]->swimThreat.data();
	} else {
		threatArray = pThreatData.load()->roleThreatPtrs[role]->surfThreat.data();
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
	CCircuitDef::RoleT role = unit->GetCircuitDef()->GetMainRole();
	if (unit->GetCircuitDef()->IsAbleToFly()) {
		return pThreatData.load()->roleThreatPtrs[role]->airThreat[z * width + x] - THREAT_BASE;
	}
	if (unit->GetCircuitDef()->IsAbleToDive()) {
		return pThreatData.load()->roleThreatPtrs[role]->amphThreat[z * width + x] - THREAT_BASE;
	}
	if (unit->GetCircuitDef()->IsAbleToSwim()) {
		return pThreatData.load()->roleThreatPtrs[role]->swimThreat[z * width + x] - THREAT_BASE;
	}
	return pThreatData.load()->roleThreatPtrs[role]->surfThreat[z * width + x] - THREAT_BASE;
}

float CThreatMap::GetUnitPower(CCircuitUnit* unit) const
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

void CThreatMap::AddEnemyUnit(SEnemyData& e)
{
	const AIFloat3 velLead = e.vel * FRAMES_PER_SEC * 1;
	const float sqVelLeadLen = velLead.SqLength2D();
	if (sqVelLeadLen > 1e-3f) {
		const AIFloat3 lead = sqVelLeadLen < SQUARE(DEFAULT_SLACK * 2)
				? velLead
				: AIFloat3(AIFloat3(e.vel).Normalize2D() * (DEFAULT_SLACK * 2));
		e.thrPos = e.pos + lead;
		CTerrainManager::CorrectPosition(e.thrPos);
	} else {
		e.thrPos = e.pos;
	}

	CCircuitDef* cdef = e.cdef;
	if (cdef == nullptr) {
		airDraws.push_back(&e);
		amphDraws.push_back(&e);
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
}

void CThreatMap::AddEnemyAir(const float threat, float* drawAirThreat,
		const SEnemyData& e, const int slack)
{
	int posx, posz;
	PosToXZ(e.thrPos, posx, posz);

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

void CThreatMap::AddEnemyAmphConst(const float threatSurf, const float threatWater,
		float* drawSurfThreat, float* drawAmphThreat, float* drawSwimThreat,
		const SEnemyData& e, const int slack)
{
	int posx, posz;
	PosToXZ(e.thrPos, posx, posz);

	int r = e.GetRange(CCircuitDef::ThreatType::SURF);
	const int rangeSurf = (r > 0) ? r + slack : 0;
	const int rangeSurfSq = (r > 0) ? SQUARE(rangeSurf) : -1;
	r = e.GetRange(CCircuitDef::ThreatType::WATER);
	const int rangeWater = (r > 0) ? r + slack : 0;
	const int rangeWaterSq = (r > 0) ? SQUARE(rangeWater) : -1;
	const int range = std::max(rangeSurf, rangeWater);
	const std::vector<SSector>& sector = areaData->sector;

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

			if (sector[index].isWater) {
				if (sum <= rangeSurfSq) {
					drawSurfThreat[index] += threatSurf;
					if (sector[index].position.y >= -SQUARE_SIZE * 5) {  // check head sticking out of water
						drawAmphThreat[index] += threatSurf;
					}
					drawSwimThreat[index] += threatSurf;
				}
				if ((sum <= rangeWaterSq) && (sector[index].position.y < -SQUARE_SIZE * 2)) {  // check minimum depth
					drawAmphThreat[index] += threatWater;
					drawSwimThreat[index] += threatWater;
				}
			} else if (sum <= rangeSurfSq) {
				drawSurfThreat[index] += threatSurf;
				drawAmphThreat[index] += threatSurf;
				drawSwimThreat[index] += threatSurf;
			}
		}
	}
}

void CThreatMap::AddEnemyAmphGradient(const float threatSurf, const float threatWater,
		float* drawSurfThreat, float* drawAmphThreat, float* drawSwimThreat,
		const SEnemyData& e, const int slack)
{
	int posx, posz;
	PosToXZ(e.thrPos, posx, posz);

	int r = e.GetRange(CCircuitDef::ThreatType::SURF);
	const int rangeLand = (r > 0) ? r + slack : 0;
	const int rangeLandSq = (r > 0) ? SQUARE(rangeLand) : -1;
	r = e.GetRange(CCircuitDef::ThreatType::WATER);
	const int rangeWater = (r > 0) ? r + slack : 0;
	const int rangeWaterSq = (r > 0) ? SQUARE(rangeWater) : -1;
	const int range = std::max(rangeLand, rangeWater);
	const std::vector<SSector>& sector = areaData->sector;

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
			const float half = 0.5f * sqrtf(sum);

			if (sector[index].isWater) {
				if (sum <= rangeLandSq) {
					const float heatLand = threatSurf * (1.0f - half / rangeLand);
					drawSurfThreat[index] += heatLand;
					if (sector[index].position.y >= -SQUARE_SIZE * 5) {  // check head sticking out of water
						drawAmphThreat[index] += heatLand;
					}
					drawSwimThreat[index] += heatLand;
				}
				if ((sum <= rangeWaterSq) && (sector[index].position.y < -SQUARE_SIZE * 2)) {  // check minimum depth
					const float heatWater = threatWater * (1.0f - half / rangeWater);
					drawAmphThreat[index] += heatWater;
					drawSwimThreat[index] += heatWater;
				}
			} else if (sum <= rangeLandSq) {
				const float heatLand = threatSurf * (1.0f - half / rangeLand);
				drawSurfThreat[index] += heatLand;
				drawAmphThreat[index] += heatLand;
				drawSwimThreat[index] += heatLand;
			}
		}
	}
}

void CThreatMap::AddDecloaker(float* drawCloakThreat, const SEnemyData& e)
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

void CThreatMap::AddShield(float* drawShieldArray, const SEnemyData& e)
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

float CThreatMap::GetThreatHealth(const CEnemyUnit* e) const
{
//	if (e->IsBeingBuilt()) {
//		return 0.f;
//	}
	const float health = e->GetHealth();
	if (health <= .0f) {
		return 0.f;
	}
	return health;
}

std::shared_ptr<IMainJob> CThreatMap::AirDrawer(CCircuitDef::RoleT role)
{
	SThreatData& threatData = *GetNextThreatData();
	SRoleThreat& roleThreat = threatData.roleThreats[role];
	std::fill(roleThreat.airThreat.begin(), roleThreat.airThreat.end(), THREAT_BASE);
	float* drawAirThreat = roleThreat.airThreat.data();

	for (const SEnemyData* e : airDraws) {
		if (e->cdef == nullptr) {
			AddEnemyAir(0.1f, drawAirThreat, *e);  // unknown enemy is a threat
		} else {
			const float threat = e->GetAirDamage(role) * e->thrHealth;
			const int vsl = std::min(int(e->vel.Length2D() * slackMod.speedMod), slackMod.speedModMax);
			AddEnemyAir(threat, drawAirThreat, *e, vsl);
		}
	}
	return ApplyDrawers();
}

std::shared_ptr<IMainJob> CThreatMap::AmphDrawer(CCircuitDef::RoleT role)
{
	SThreatData& threatData = *GetNextThreatData();
	SRoleThreat& roleThreat = threatData.roleThreats[role];
	std::fill(roleThreat.surfThreat.begin(), roleThreat.surfThreat.end(), THREAT_BASE);
	std::fill(roleThreat.amphThreat.begin(), roleThreat.amphThreat.end(), THREAT_BASE);
	std::fill(roleThreat.swimThreat.begin(), roleThreat.swimThreat.end(), THREAT_BASE);
	float* drawSurfThreat = roleThreat.surfThreat.data();
	float* drawAmphThreat = roleThreat.amphThreat.data();
	float* drawSwimThreat = roleThreat.swimThreat.data();

	for (const SEnemyData* e : amphDraws) {
		if (e->cdef == nullptr) {
			AddEnemyAmphGradient(0.1f, 0.1f, drawSurfThreat, drawAmphThreat, drawSwimThreat, *e);  // unknown enemy is a threat
		} else {
			const float threatSurf = e->GetSurfDamage(role) * e->thrHealth;
			const float threatWater = e->GetWaterDamage(role) * e->thrHealth;
			const int vsl = std::min(int(e->vel.Length2D() * slackMod.speedMod), slackMod.speedModMax);
			e->cdef->IsAlwaysHit()
					? AddEnemyAmphConst(threatSurf, threatWater, drawSurfThreat, drawAmphThreat, drawSwimThreat, *e, vsl)
					: AddEnemyAmphGradient(threatSurf, threatWater, drawSurfThreat, drawAmphThreat, drawSwimThreat, *e, vsl);
		}
	}
	return ApplyDrawers();
}

std::shared_ptr<IMainJob> CThreatMap::ApplyDrawers()
{
#ifdef CHRONO_THREAT
	if (--numThreadDraws == 0) {
		int count = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - t0).count();
		printf("%i | %i mcs\n", manager->GetCircuit()->GetSkirmishAIId(), count);
		return CScheduler::GameJob(&CThreatMap::Apply, this);
	} else {
		return nullptr;
	}
#else
	return (--numThreadDraws == 0) ? CScheduler::GameJob(&CThreatMap::Apply, this) : nullptr;
#endif
}

std::shared_ptr<IMainJob> CThreatMap::Update(CEnemyManager* enemyMgr, CScheduler* scheduler)
{
#ifdef CHRONO_THREAT
	t0 = clock::now();
#endif
	airDraws.clear();
	amphDraws.clear();
	for (const SEnemyData& e : enemyMgr->GetHostileDatas()) {
		AddEnemyUnit(const_cast<SEnemyData&>(e));
	}

	SThreatData& threatData = *GetNextThreatData();
	numThreadDraws = 1 + 2 * threatData.roleThreats.size();
	for (auto& kv : threatData.roleThreats) {
		scheduler->RunPriorityJob(CScheduler::WorkJob(&CThreatMap::AirDrawer, this, kv.first));
		scheduler->RunPriorityJob(CScheduler::WorkJob(&CThreatMap::AmphDrawer, this, kv.first));
	}

	std::fill(threatData.cloakThreat.begin(), threatData.cloakThreat.end(), THREAT_BASE);
	std::fill(threatData.shield.begin(), threatData.shield.end(), 0.f);
	float* drawCloakThreat = threatData.cloakThreat.data();
	float* drawShieldArray = threatData.shield.data();

	for (const std::vector<SEnemyData>& datas : {enemyMgr->GetHostileDatas(), enemyMgr->GetPeaceDatas()}) {
		for (const SEnemyData& e : datas) {
			AddDecloaker(drawCloakThreat, e);
			if ((e.cdef != nullptr) && (e.cdef->GetShieldMount() != nullptr)) {
				AddShield(drawShieldArray, e);
			}
		}
	}

	return ApplyDrawers();
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
	cloakThreat = threatData.cloakThreat.data();
	shieldArray = threatData.shield.data();
	threatArray = threatData.defThreat->surfThreat.data();
}

#ifdef DEBUG_VIS
void CThreatMap::UpdateVis()
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (isWidgetDrawing || isWidgetPrinting) {
		std::ostringstream cmd;
		cmd << "ai_thr_data:";
		float* threatArray;
		switch (layerDbg) {
			case 0: {
				threatArray = pThreatData.load()->roleThreats[0].airThreat.data();
			} break;
			default:
			case 1: {
				threatArray = pThreatData.load()->roleThreats[0].surfThreat.data();
			} break;
			case 2: {
				threatArray = pThreatData.load()->roleThreats[0].amphThreat.data();
			} break;
			case 3: {
				threatArray = pThreatData.load()->roleThreats[0].swimThreat.data();
			} break;
		}
		cmd.write(reinterpret_cast<const char*>(threatArray), mapSize * sizeof(float));
		std::string s = cmd.str();
		circuit->GetLua()->CallRules(s.c_str(), s.size());
	}

	if (sdlWindows.empty()/* || (currMaxThreat < .1f)*/) {
		return;
	}

	Uint32 sdlWindowId;
	float* dbgMap;
	std::tie(sdlWindowId, dbgMap) = sdlWindows[0];
	float* airThreat = pThreatData.load()->roleThreats[0].airThreat.data();
	for (int i = 0; i < mapSize; ++i) {
		dbgMap[i] = std::min<float>((airThreat[i] - THREAT_BASE) / 40.0f /*currMaxThreat*/, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap);

	std::tie(sdlWindowId, dbgMap) = sdlWindows[1];
	float* surfThreat = pThreatData.load()->roleThreats[0].surfThreat.data();
	for (int i = 0; i < mapSize; ++i) {
		dbgMap[i] = std::min<float>((surfThreat[i] - THREAT_BASE) / 40.0f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap);

	std::tie(sdlWindowId, dbgMap) = sdlWindows[2];
	float* amphThreat = pThreatData.load()->roleThreats[0].amphThreat.data();
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

void CThreatMap::SetMaxThreat(float maxThreat, std::string layer)
{
	if (layer == "air") {
		layerDbg = 0;
	} else if (layer == "amph") {
		layerDbg = 2;
	} else if (layer == "swim") {
		layerDbg = 3;
	} else {  // surf
		layerDbg = 1;
	}
	CCircuitAI* circuit = manager->GetCircuit();
	std::string cmd = utils::float_to_string(maxThreat, "ai_thr_div:%f");
	circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());
}
#endif  // DEBUG_VIS

} // namespace circuit
