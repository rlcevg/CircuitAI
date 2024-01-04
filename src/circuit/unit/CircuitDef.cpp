/*
 * CircuitDef.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "unit/CircuitDef.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"

#include "WeaponMount.h"
#include "WeaponDef.h"
#include "Damage.h"
#include "Shield.h"
#include "MoveData.h"
#include "Map.h"
#include "Log.h"

#include <regex>

namespace circuit {

using namespace springai;

#define THREAT_MOD		(1.0f / 128.0f)

CCircuitDef::RoleName* CCircuitDef::roleNames;
CCircuitDef::AttrName CCircuitDef::attrNames = {
	{"melee",     CCircuitDef::AttrType::MELEE},
	{"boost",     CCircuitDef::AttrType::BOOST},
	{"no_jump",   CCircuitDef::AttrType::NO_JUMP},
	{"no_strafe", CCircuitDef::AttrType::NO_STRAFE},
	{"stockpile", CCircuitDef::AttrType::STOCK},
	{"siege",     CCircuitDef::AttrType::SIEGE},
	{"ret_hold",  CCircuitDef::AttrType::RET_HOLD},
	{"ret_fight", CCircuitDef::AttrType::RET_FIGHT},
	{"jump",      CCircuitDef::AttrType::JUMP},
	{"dg_cost",   CCircuitDef::AttrType::DG_COST},
	{"dg_still",  CCircuitDef::AttrType::DG_STILL},
	{"anti_stat", CCircuitDef::AttrType::ANTI_STAT},
	{"rearm",     CCircuitDef::AttrType::REARM},
};
CCircuitDef::FireName CCircuitDef::fireNames = {
	{"hold",   CCircuitDef::FireType::HOLD},
	{"return", CCircuitDef::FireType::RETURN},
	{"open",   CCircuitDef::FireType::OPEN},
};

void CCircuitDef::InitStatic(CCircuitAI* circuit, CMaskHandler* roleMasker)
{
	std::vector<std::pair<std::string, CMaskHandler::TypeMask>> roles = {
		{"",           {ROLE_TYPE(NONE),    CCircuitDef::RoleMask::NONE}},
		{"builder",    {ROLE_TYPE(BUILDER), CCircuitDef::RoleMask::BUILDER}},
		{"scout",      {ROLE_TYPE(SCOUT),   CCircuitDef::RoleMask::SCOUT}},
		{"raider",     {ROLE_TYPE(RAIDER),  CCircuitDef::RoleMask::RAIDER}},
		{"riot",       {ROLE_TYPE(RIOT),    CCircuitDef::RoleMask::RIOT}},
		{"assault",    {ROLE_TYPE(ASSAULT), CCircuitDef::RoleMask::ASSAULT}},
		{"skirmish",   {ROLE_TYPE(SKIRM),   CCircuitDef::RoleMask::SKIRM}},
		{"artillery",  {ROLE_TYPE(ARTY),    CCircuitDef::RoleMask::ARTY}},
		{"anti_air",   {ROLE_TYPE(AA),      CCircuitDef::RoleMask::AA}},
		{"anti_sub",   {ROLE_TYPE(AS),      CCircuitDef::RoleMask::AS}},
		{"anti_heavy", {ROLE_TYPE(AH),      CCircuitDef::RoleMask::AH}},
		{"bomber",     {ROLE_TYPE(BOMBER),  CCircuitDef::RoleMask::BOMBER}},
		{"support",    {ROLE_TYPE(SUPPORT), CCircuitDef::RoleMask::SUPPORT}},
		{"mine",       {ROLE_TYPE(MINE),    CCircuitDef::RoleMask::MINE}},
		{"transport",  {ROLE_TYPE(TRANS),   CCircuitDef::RoleMask::TRANS}},
		{"air",        {ROLE_TYPE(AIR),     CCircuitDef::RoleMask::AIR}},
		{"sub",        {ROLE_TYPE(SUB),     CCircuitDef::RoleMask::SUB}},
		{"static",     {ROLE_TYPE(STATIC),  CCircuitDef::RoleMask::STATIC}},
		{"heavy",      {ROLE_TYPE(HEAVY),   CCircuitDef::RoleMask::HEAVY}},
		{"super",      {ROLE_TYPE(SUPER),   CCircuitDef::RoleMask::SUPER}},
		{"commander",  {ROLE_TYPE(COMM),    CCircuitDef::RoleMask::COMM}},
	};
	for (auto& kv : roles) {
		CMaskHandler::TypeMask tm = roleMasker->GetTypeMask(kv.first);
		if ((tm.type != kv.second.type) || (tm.mask != kv.second.mask)) {
			circuit->LOG("RoleError: %s = (%i, 0x%08X) != (%i, 0x%08X)", kv.first.c_str(),
						 kv.second.type, kv.second.mask, tm.type, tm.mask);
		}
	}

	CCircuitDef::roleNames = &roleMasker->GetMasks();
}

CCircuitDef::CCircuitDef(CCircuitAI* circuit, UnitDef* def, std::unordered_set<Id>& buildOpts,
		Resource* resM, Resource* resE)
		: def(def)
		, mainRole(ROLE_TYPE(ASSAULT))
		, enemyRole(RoleMask::NONE)
		, respRole(RoleMask::NONE)
		, role(RoleMask::NONE)
		, attr(NONE)
		, buildOptions(buildOpts)
		, count(0)
		, buildCounts(0)
		, sinceFrame(-1)
		, dgunDef(nullptr)
		, dgunMount(nullptr)
		, shieldMount(nullptr)
		, weaponMount(nullptr)
		, pwrDmg(.0f)
		, thrDmg(.0f)
		, aoe(.0f)
		, power(.0f)
		, threat(.0f)
		, minRange(.0f)
		, maxRangeType(RangeType::AIR)
		, shieldRadius(.0f)
		, maxShield(.0f)
		, reloadTime(0)
		, targetCategory(0)
		, immobileTypeId(-1)
		, mobileTypeId(-1)
		, isIgnore(false)
		, isAttacker(false)
		, hasDGunAA(false)
		, hasAntiAir(false)
		, hasAntiLand(false)
		, hasAntiWater(false)
		, isAlwaysHit(false)
		, isAmphibious(false)
		, isLander(false)
		, isMex(false)
		, isPylon(false)
		, isAssist(false)
		, stockCost(.0f)
		, jumpRange(.0f)
		, retreat(-1.f)
		, radius(-1.f)
		, height(-1.f)
		, topOffset(-1.f)
{
	maxRange.fill(.0f);
	threatRange.fill(0);

	id = def->GetUnitDefId();

	buildDistance = def->GetBuildDistance();
	buildSpeed    = def->GetBuildSpeed();
	maxThisUnit   = def->GetMaxThisUnit();

//	maxRange[static_cast<RangeT>(RangeType::MAX)] = def->GetMaxWeaponRange();
	hasDGun         = def->CanManualFire();
	category        = def->GetCategory();
	noChaseCategory = (def->GetNoChaseCategory() | circuit->GetBadCategory())
					  & ~circuit->GetGoodCategory();

	const int ft = def->GetFireState();
	fireState = (ft < 0) ? FireType::OPEN : static_cast<FireType>(ft);

	health    = def->GetHealth();
	speed     = def->GetSpeed();  // elmos per second
	losRadius = def->GetLosRadius();
	costM     = def->GetCost(resM);
	costE     = def->GetCost(resE);
	cloakCost = std::max(def->GetCloakCost(), def->GetCloakCostMoving());
	buildTime = def->GetBuildTime();
//	altitude  = def->GetWantedHeight();

	COOAICallback* clb = circuit->GetCallback();
	MoveData* md = def->GetMoveData();
	isSubmarine  = (md == nullptr) ? false : md->IsSubMarine();
	delete md;
	isAbleToFly     = def->IsAbleToFly();
	isPlane         = !def->IsHoverAttack() && isAbleToFly;
	isFloater       = def->IsFloater() && !isSubmarine && !isAbleToFly;
	isSonarStealth  = def->IsSonarStealth();
	isTurnLarge     = (speed / (def->GetTurnRate() + 1e-3f) > 0.09f);  // empirical magic number
	isAbleToCloak   = def->IsAbleToCloak();
	isAbleToRepair  = def->IsAbleToRepair();
	isAbleToReclaim = def->IsAbleToReclaim();
	isAbleToAssist  = def->IsAbleToAssist() && !clb->UnitDef_HasYardMap(id);
	// Factory: def->IsBuilder() && !def->GetYardMap(0).empty() && !def->GetBuildOptions().empty()

	const std::map<std::string, std::string>& customParams = def->GetCustomParams();
	auto it = customParams.find("canjump");
	isAbleToJump = (it != customParams.end()) && (utils::string_to_int(it->second) == 1);
	if (isAbleToJump) {
		it = customParams.find("jump_range");
		jumpRange = (it != customParams.end()) ? utils::string_to_float(it->second) : 400.0f;
	}

	it = customParams.find("is_drone");
	if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
		category |= circuit->GetBadCategory();
		costM *= 0.1f;  // avoid threat metal
	}

//	if (customParams.find("boost_speed_mult") != customParams.end()) {
//		AddAttribute(AttrType::BOOST);
//	}

	bool isDynamic = false;
	if (customParams.find("level") != customParams.end()) {
		isDynamic = customParams.find("dynamic_comm") != customParams.end();
		AddRole(ROLE_TYPE(COMM));
	}

	it = customParams.find("midposoffset");
	if (it != customParams.end()) {
		const std::string& str = it->second;
		std::string::const_iterator start = str.begin();
		std::string::const_iterator end = str.end();
		std::regex pattern("(-?\\d+)");
		std::smatch section;
		int index = 0;
		while (std::regex_search(start, end, section, pattern) && (index < 3)) {
			midPosOffset[index++] = utils::string_to_float(section[1]);
			start = section[0].second;
		}
	} else {
		midPosOffset = ZeroVector;
	}

	WeaponDef* sd = def->GetShieldDef();
	const bool isShield = (sd != nullptr);
	if (isShield) {
		Shield* shield = sd->GetShield();
		shieldRadius = shield->GetRadius();
		maxShield = shield->GetPower();
		delete shield;
	}
	delete sd;

	WeaponDef* stockDef = def->GetStockpileDef();
	if (stockDef != nullptr) {
		it = customParams.find("stockpilecost");
		if (it != customParams.end()) {
			stockCost = utils::string_to_float(it->second);
		}
		AddAttribute(AttrType::STOCK);
		delete stockDef;
	}

	if (!def->IsAbleToAttack()) {
		if (isShield) {
			auto mounts = def->GetWeaponMounts();
			for (WeaponMount* mount : mounts) {
				WeaponDef* wd = mount->GetWeaponDef();
				if ((shieldMount == nullptr) && wd->IsShield()) {
					shieldMount = mount;  // NOTE: Unit may have more than 1 shield
				} else {
					delete mount;
				}
				delete wd;
			}
		}
		// NOTE: Aspis (mobile shield) has 10 damage for some reason, break
		return;
	}

	/*
	 * DPS and Weapon calculations
	 */
	std::set<CWeaponDef::Id> allWeaponDefs;
	minRange = std::numeric_limits<float>::max();
	float minReloadTime = std::numeric_limits<float>::max();
	float bestDGunReload = std::numeric_limits<float>::max();
	float bestWpRange = std::numeric_limits<float>::max();
	float dps = .0f;  // TODO: split dps like ranges on air, land, water
	float dmg = .0f;
	CWeaponDef* bestDGunDef = nullptr;
	WeaponMount* bestDGunMnt = nullptr;
	WeaponMount* bestWpMnt = nullptr;
	bool canTargetAir = false;
	bool canTargetLand = false;
	bool canTargetWater = false;
	auto mounts = def->GetWeaponMounts();
	for (WeaponMount* mount : mounts) {
		WeaponDef* wd = mount->GetWeaponDef();
		const std::map<std::string, std::string>& customParams = wd->GetCustomParams();

		if (customParams.find("fake_weapon") != customParams.end()) {
			delete wd;
			delete mount;
			continue;
		}

		float scale = wd->IsParalyzer() ? 0.5f : 1.0f;

		float extraDmg = .0f;
		auto it = customParams.find("extra_damage");
		if (it != customParams.end()) {
			extraDmg += utils::string_to_float(it->second);
		}

		it = customParams.find("disarmdamageonly");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			scale = 0.5f;
		}

		it = customParams.find("timeslow_onlyslow");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			scale = 0.5f;
		} else {
			it = customParams.find("timeslow_damagefactor");
			if (it != customParams.end()) {
				scale = utils::string_to_float(it->second);
			}
		}

		it = customParams.find("is_capture");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			scale = 2.0f;
		}

		it = customParams.find("area_damage_dps");
		if (it != customParams.end()) {
			extraDmg += utils::string_to_float(it->second);
			it = customParams.find("area_damage_is_impulse");
			if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
				scale = 0.02f;
			}
		}

		if (customParams.find("reammoseconds") != customParams.end()) {
			AddAttribute(AttrType::REARM);
		}

		float reloadTime = wd->GetReload();  // seconds
		if (minReloadTime > reloadTime) {
			minReloadTime = reloadTime;
		}
		if (extraDmg > 0.1f) {
			dmg += extraDmg;
			dps += extraDmg * wd->GetSalvoSize() / reloadTime * scale;
		}

		float ldmg = .0f;
		it = customParams.find("statsdamage");
		if (it != customParams.end()) {
			ldmg = utils::string_to_float(it->second);
		} else {
			Damage* damage = wd->GetDamage();
			const std::vector<float>& damages = damage->GetTypes();
			delete damage;
			for (float d : damages) {
				ldmg += d;
			}
			ldmg /= damages.size();
		}
		ldmg *= std::pow(2.0f, (wd->IsDynDamageInverted() ? 1 : -1) * wd->GetDynDamageExp());
		dmg += ldmg;
		dps += ldmg * wd->GetSalvoSize() / reloadTime * scale;
		int weaponCat = mount->GetOnlyTargetCategory();
		targetCategory |= weaponCat;

		aoe = std::max(aoe, wd->GetAreaOfEffect());

		std::string wt(wd->GetType());  // @see https://springrts.com/wiki/Gamedev:WeaponDefs
		const float projectileSpeed = wd->GetProjectileSpeed();
		it = customParams.find("truerange");
		float range = (it != customParams.end()) ? utils::string_to_float(it->second) : wd->GetRange();

		isAlwaysHit |= ((wt == "Cannon") || (wt == "DGun") || (wt == "EmgCannon") || (wt == "Flame") ||
				(wt == "LaserCannon") || (wt == "AircraftBomb")) && (projectileSpeed * FRAMES_PER_SEC >= .8f * range);  // Cannons with fast projectiles
		isAlwaysHit |= (wt == "BeamLaser") || (wt == "LightningCannon") || (wt == "Rifle") ||  // Instant-hit
				(((wt == "MissileLauncher") || (wt == "StarburstLauncher") || ((wt == "TorpedoLauncher") && wd->IsSubMissile())) && wd->IsTracks());  // Missiles
		const bool isAirWeapon = isAlwaysHit && (range > 150.f);
		canTargetAir |= isAirWeapon;

		bool isLandWeapon = ((wt != "TorpedoLauncher") || wd->IsSubMissile());
		canTargetLand |= isLandWeapon;
		bool isWaterWeapon = wd->IsWaterWeapon();
		canTargetWater |= isWaterWeapon;

		minRange = std::min(minRange, range);
		if ((weaponCat & circuit->GetAirCategory()) && isAirWeapon) {
			float& mr = maxRange[static_cast<RangeT>(RangeType::AIR)];
			mr = std::max(mr, range);
			if (mr > maxRange[static_cast<RangeT>(maxRangeType)]) {
				maxRangeType = RangeType::AIR;
			}
		}
		if ((weaponCat & circuit->GetLandCategory()) && isLandWeapon) {
			float& mr = maxRange[static_cast<RangeT>(RangeType::LAND)];
			mr = std::max(mr, (isAbleToFly && (wt == "Cannon")) ? range * 1.25f : range);  // 1.25 - gunship height hax
			if (mr > maxRange[static_cast<RangeT>(maxRangeType)]) {
				maxRangeType = RangeType::LAND;
			}
		}
		if ((weaponCat & circuit->GetWaterCategory()) && isWaterWeapon) {
			float& mr = maxRange[static_cast<RangeT>(RangeType::WATER)];
			mr = std::max(mr, range);
			if (mr > maxRange[static_cast<RangeT>(maxRangeType)]) {
				maxRangeType = RangeType::WATER;
			}
		}

		if (wd->IsManualFire() && (reloadTime < bestDGunReload)) {
			// NOTE: Disable commander's dgun, because no usage atm
			if (customParams.find("manualfire") == customParams.end()) {
				bestDGunReload = reloadTime;
				bestDGunDef = circuit->GetWeaponDef(wd->GetWeaponDefId());
				delete bestDGunMnt;
				bestDGunMnt = mount;
				hasDGunAA |= (weaponCat & circuit->GetAirCategory()) && isAirWeapon;
			} else {  // FIXME: Dynamo com workaround
				delete mount;
			}
		} else if (wd->IsShield()) {
			if (shieldMount == nullptr) {
				shieldMount = mount;  // NOTE: Unit may have more than 1 shield
			} else {
				delete mount;
			}
		} else if (range < bestWpRange) {
			delete bestWpMnt;
			bestWpMnt = mount;
			bestWpRange = range;
		} else {
			delete mount;
		}

		allWeaponDefs.insert(wd->GetWeaponDefId());

		delete wd;
	}

	circuit->BindUnitToWeaponDefs(GetId(), allWeaponDefs, IsMobile());

	if (isDynamic) {  // FIXME: Dynamo com workaround
		dps /= mounts.size();
		dmg /= mounts.size();
	}

	if (minReloadTime < std::numeric_limits<float>::max()) {
		reloadTime = minReloadTime * FRAMES_PER_SEC;
	}
	if (bestDGunReload < std::numeric_limits<float>::max()) {
		dgunDef = bestDGunDef;
		dgunMount = bestDGunMnt;
	}
	if (bestWpRange < std::numeric_limits<float>::max()) {
		weaponMount = bestWpMnt;
	}

	isAttacker = dps > .1f;
	if (IsMobile() && !IsAttacker()) {  // mobile bomb?
		WeaponDef* wd = def->GetDeathExplosion();
		aoe = wd->GetAreaOfEffect();
		if (aoe > 64.0f) {
			// power
			float ldmg = .0f;
			it = customParams.find("statsdamage");
			if (it != customParams.end()) {
				ldmg = utils::string_to_float(it->second);
			} else {
				Damage* damage = wd->GetDamage();
				const std::vector<float>& damages = damage->GetTypes();
				delete damage;
				for (float d : damages) {
					ldmg += d;
				}
				ldmg /= damages.size();
			}
			dmg += ldmg;
			dps = ldmg * wd->GetSalvoSize();
			isAttacker = dps > .1f;
			// range
			minRange = aoe;
			for (RangeT rt = 0; rt < static_cast<RangeT>(RangeType::_SIZE_); ++rt) {
				float& mr = maxRange[rt];
				mr = std::max(mr, aoe);
			}
			// category
			targetCategory = wd->GetOnlyTargetCategory();  // 0xFFFFFFFF
			if (~targetCategory == 0) {
				targetCategory = ~circuit->GetBadCategory();
			}
			category |= circuit->GetBadCategory();  // do not chase bombs
		}
		delete wd;
	}

	// NOTE: isTracks filters units with slow weapon (hermit, recluse, rocko)
	hasAntiAir   = (targetCategory & circuit->GetAirCategory()) && canTargetAir;
	hasAntiLand  = (targetCategory & circuit->GetLandCategory()) && canTargetLand;
	hasAntiWater = (targetCategory & circuit->GetWaterCategory()) && canTargetWater;

	// TODO: Include projectile-speed/range, armor
	//       health /= def->GetArmoredMultiple();
	thrDmg = pwrDmg = dmg = sqrtf(dps) * std::pow(dmg, 0.25f) * THREAT_MOD;
	threat = power = dmg * sqrtf(def->GetHealth() + maxShield * SHIELD_MOD);
}

CCircuitDef::~CCircuitDef()
{
	delete def;
	delete dgunMount;
	delete shieldMount;
	delete weaponMount;
}

void CCircuitDef::Init(CCircuitAI* circuit)
{
	CTerrainData& terrainData = circuit->GetGameAttribute()->GetTerrainData();
	assert(terrainData.IsInitialized());

	if (IsAbleToFly()) {

	} else if (!IsMobile()) {  // for immobile units

		immobileTypeId = terrainData.udImmobileType[GetId()];
		// If a unit can build mobile units then it will inherit mobileType from it's options
		std::map<STerrainMapMobileType::Id, float> mtUsability;
		for (CCircuitDef::Id buildId : GetBuildOptions()) {
			CCircuitDef* bdef = circuit->GetCircuitDef(buildId);
			if ((bdef == nullptr) || !bdef->IsMobile() || !bdef->IsAttacker()) {
				continue;
			}
			STerrainMapMobileType::Id mtId = terrainData.udMobileType[bdef->GetId()];
			if ((mtId < 0) || (mtUsability.find(mtId) != mtUsability.end())) {
				continue;
			}
			STerrainMapMobileType& mt = terrainData.areaData0.mobileType[mtId];
			mtUsability[mtId] = mt.area.empty() ? 00.0 : mt.areaLargest->percentOfMap;
		}
		float useMost = .0f;
		STerrainMapMobileType::Id mtId = mobileTypeId;  // -1
		for (auto& mtkv : mtUsability) {
			if (mtkv.second > useMost) {
				mtId = mtkv.first;
				useMost = mtkv.second;
			}
		}
		mobileTypeId = mtId;

	} else {  // for mobile units

		mobileTypeId = terrainData.udMobileType[GetId()];
	}

	if (IsMobile()) {
		if (mobileTypeId >= 0) {
			STerrainMapMobileType& mt = terrainData.areaData0.mobileType[mobileTypeId];
			isAmphibious = ((mt.minElevation < -SQUARE_SIZE * 5) || (mt.maxElevation < SQUARE_SIZE * 5)) && !IsFloater();
		}
	} else {
		if (immobileTypeId >= 0) {
			STerrainMapImmobileType& it = terrainData.areaData0.immobileType[immobileTypeId];
			isAmphibious = ((it.minElevation < -SQUARE_SIZE * 5) || (it.maxElevation < SQUARE_SIZE * 5)) && !IsFloater();
		}
	}
	isLander = !IsFloater() && !IsAbleToFly() && !IsAmphibious() && !IsSubmarine();
}

void CCircuitDef::AddRole(RoleT type, RoleT bindType)
{
	respRole |= GetMask(type);
	role |= GetMask(bindType);
}

float CCircuitDef::GetRadius()
{
	if (radius < 0.f) {
		radius = def->GetRadius();  // Forces loading of the unit model
	}
	return radius;
}

float CCircuitDef::GetHeight()
{
	if (height < 0.f) {
		height    = def->GetHeight();  // Forces loading of the unit model
		topOffset = height / 2 - def->GetWaterline();
	}
	return height;
}

bool CCircuitDef::IsYTargetable(float elevation, float posY) {
	GetHeight();
	return (elevation > -height || posY > -topOffset);
}

} // namespace circuit
