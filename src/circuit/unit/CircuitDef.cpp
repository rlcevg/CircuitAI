/*
 * CircuitDef.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "unit/CircuitDef.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/utils.h"

#include "WeaponMount.h"
#include "WeaponDef.h"
#include "Damage.h"
#include "Shield.h"
#include "MoveData.h"
#include "Map.h"

#include <regex>

namespace circuit {

using namespace springai;

CCircuitDef::RoleName CCircuitDef::roleNames = {
	{"builder",    CCircuitDef::RoleType::BUILDER},
	{"scout",      CCircuitDef::RoleType::SCOUT},
	{"raider",     CCircuitDef::RoleType::RAIDER},
	{"riot",       CCircuitDef::RoleType::RIOT},
	{"assault",    CCircuitDef::RoleType::ASSAULT},
	{"skirmish",   CCircuitDef::RoleType::SKIRM},
	{"artillery",  CCircuitDef::RoleType::ARTY},
	{"anti_air",   CCircuitDef::RoleType::AA},
	{"anti_sub",   CCircuitDef::RoleType::AS},
	{"anti_heavy", CCircuitDef::RoleType::AH},
	{"bomber",     CCircuitDef::RoleType::BOMBER},
	{"support",    CCircuitDef::RoleType::SUPPORT},
	{"mine",       CCircuitDef::RoleType::MINE},
	{"transport",  CCircuitDef::RoleType::TRANS},
	{"air",        CCircuitDef::RoleType::AIR},
	{"sub",        CCircuitDef::RoleType::SUB},
	{"static",     CCircuitDef::RoleType::STATIC},
	{"heavy",      CCircuitDef::RoleType::HEAVY},
};

CCircuitDef::AttrName CCircuitDef::attrNames = {
	{"melee",     CCircuitDef::AttrType::MELEE},
	{"siege",     CCircuitDef::AttrType::SIEGE},
	{"open_fire", CCircuitDef::AttrType::OPEN_FIRE},
	{"no_jump",   CCircuitDef::AttrType::NO_JUMP},
	{"boost",     CCircuitDef::AttrType::BOOST},
	{"comm",      CCircuitDef::AttrType::COMM},
	{"hold_fire", CCircuitDef::AttrType::HOLD_FIRE},
	{"no_strafe", CCircuitDef::AttrType::NO_STRAFE},
	{"stockpile", CCircuitDef::AttrType::STOCK},
	{"super",     CCircuitDef::AttrType::SUPER},
};

CCircuitDef::CCircuitDef(CCircuitAI* circuit, UnitDef* def, std::unordered_set<Id>& buildOpts, Resource* res)
		: def(def)
		, mainRole(RoleType::SCOUT)
		, enemyRole(RoleType::SCOUT)
		, role(RoleMask::NONE)
		, buildOptions(buildOpts)
		, count(0)
		, buildCounts(0)
		, hasDGunAA(false)
//		, dgunReload(-1)
		, dgunRange(.0f)
		, dgunMount(nullptr)
		, shieldMount(nullptr)
		, weaponMount(nullptr)
		, dps(.0f)
		, dmg(.0f)
		, aoe(.0f)
		, power(.0f)
		, minRange(.0f)
		, maxRange({.0f})
		, maxShield(.0f)
		, reloadTime(0)
		, targetCategory(0)
		, immobileTypeId(-1)
		, mobileTypeId(-1)
		, hasAntiAir(false)
		, hasAntiLand(false)
		, hasAntiWater(false)
		, isAmphibious(false)
		, isLander(false)
		, stockCost(.0f)
		, jumpRange(.0f)
		, retreat(-1.f)
{
	id = def->GetUnitDefId();

	buildDistance = def->GetBuildDistance();
	buildSpeed    = def->GetBuildSpeed();
	maxThisUnit   = def->GetMaxThisUnit();

	maxRange[static_cast<RangeT>(RangeType::MAX)] = def->GetMaxWeaponRange();
	hasDGun         = def->CanManualFire();
	category        = def->GetCategory();
	noChaseCategory = (def->GetNoChaseCategory() | circuit->GetBadCategory()) & ~circuit->GetGoodCategory();

	speed     = def->GetSpeed() / FRAMES_PER_SEC;  // NOTE: SetWantedMaxSpeed expects value/FRAMES_PER_SEC
	losRadius = def->GetLosRadius();
	cost      = def->GetCost(res);
	cloakCost = std::max(def->GetCloakCost(), def->GetCloakCostMoving());
//	altitude  = def->GetWantedHeight();

	MoveData* md = def->GetMoveData();
	isSubmarine = (md == nullptr) ? false : md->IsSubMarine();
	delete md;
	isAbleToFly    = def->IsAbleToFly();
	isPlane        = !def->IsHoverAttack() && isAbleToFly;
	isFloater      = def->IsFloater() && !isSubmarine && !isAbleToFly;
	isSonarStealth = def->IsSonarStealth();
	isTurnLarge    = (speed / (def->GetTurnRate() + 1e-3f) > 0.003f);
	isAbleToCloak  = def->IsAbleToCloak();

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
		cost *= 0.1f;
	}

//	if (customParams.find("boost_speed_mult") != customParams.end()) {
//		AddAttribute(AttrType::BOOST);
//	}

	bool isDynamic = false;
	if (customParams.find("level") != customParams.end()) {
		isDynamic = customParams.find("dynamic_comm") != customParams.end();
		AddAttribute(AttrType::COMM);
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
		maxShield = shield->GetPower();
		delete shield;
	}
	delete sd;

	if (!def->IsAbleToAttack()) {
		if (isShield) {
			auto mounts = std::move(def->GetWeaponMounts());
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

	WeaponDef* stockDef = def->GetStockpileDef();
	if (stockDef != nullptr) {
		it = customParams.find("stockpilecost");
		if (it != customParams.end()) {
			stockCost = utils::string_to_float(it->second);
		}
		AddAttribute(AttrType::STOCK);
		delete stockDef;
	}

	/*
	 * DPS and Weapon calculations
	 */
	minRange = std::numeric_limits<float>::max();
	float minReloadTime = std::numeric_limits<float>::max();
	float bestDGunReload = std::numeric_limits<float>::max();
	float bestDGunRange = .0f;
	float bestWpRange = std::numeric_limits<float>::max();
	WeaponMount* bestDGunMnt = nullptr;
	WeaponMount* bestWpMnt = nullptr;
	bool canTargetAir = false;
	bool canTargetLand = false;
	bool canTargetWater = false;
	auto mounts = std::move(def->GetWeaponMounts());
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
		bool isAirWeapon = false;
		const float projectileSpeed = wd->GetProjectileSpeed();
		float range = wd->GetRange();

		isAirWeapon = ((wt == "Cannon") || (wt == "DGun") || (wt == "EmgCannon") || (wt == "Flame") ||
				(wt == "LaserCannon") || (wt == "AircraftBomb")) && (projectileSpeed * FRAMES_PER_SEC >= .75f * range);  // Cannons with fast projectiles
		isAirWeapon |= (wt == "BeamLaser") || (wt == "LightningCannon") || (wt == "Rifle") ||  // Instant-hit
				(((wt == "MissileLauncher") || (wt == "StarburstLauncher") || ((wt == "TorpedoLauncher") && wd->IsSubMissile())) && wd->IsTracks());  // Missiles
		isAirWeapon &= (range > 150.f);
		canTargetAir |= isAirWeapon;

		bool isLandWeapon = ((wt != "TorpedoLauncher") || wd->IsSubMissile());
		canTargetLand |= isLandWeapon;
		bool isWaterWeapon = wd->IsWaterWeapon();
		canTargetWater |= isWaterWeapon;

		minRange = std::min(minRange, range);
		if ((weaponCat & circuit->GetAirCategory()) && isAirWeapon) {
			float& mr = maxRange[static_cast<RangeT>(RangeType::AIR)];
			mr = std::max(mr, range);
		}
		if ((weaponCat & circuit->GetLandCategory()) && isLandWeapon) {
			float& mr = maxRange[static_cast<RangeT>(RangeType::LAND)];
			mr = std::max(mr, (isAbleToFly && (wt == "Cannon")) ? range * 1.25f : range);
		}
		if ((weaponCat & circuit->GetWaterCategory()) && isWaterWeapon) {
			float& mr = maxRange[static_cast<RangeT>(RangeType::WATER)];
			mr = std::max(mr, range);
		}

		if (wd->IsManualFire() && (reloadTime < bestDGunReload)) {
			// NOTE: Disable commander's dgun, because no usage atm
			if (customParams.find("manualfire") == customParams.end()) {
				bestDGunReload = reloadTime;
				bestDGunRange = range;
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
		delete wd;
	}
	if (isDynamic) {  // FIXME: Dynamo com workaround
		dps /= mounts.size();
		dmg /= mounts.size();
		// NOTE: minRange should be fine
		for (RangeType rt : {RangeType::AIR, RangeType::LAND, RangeType::WATER}) {
			float& mr = maxRange[static_cast<RangeT>(rt)];
			mr = std::min(mr, 400.0f);
		}
	}

	if (minReloadTime < std::numeric_limits<float>::max()) {
 		reloadTime = minReloadTime * FRAMES_PER_SEC;
	}
	if (bestDGunReload < std::numeric_limits<float>::max()) {
//		dgunReload = math::ceil(bestReload * FRAMES_PER_SEC)/* + FRAMES_PER_SEC*/;
		dgunRange = bestDGunRange;
		dgunMount = bestDGunMnt;
	}
	if (bestWpRange < std::numeric_limits<float>::max()) {
		weaponMount = bestWpMnt;
	}

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

	// TODO: Include projectile-speed/range
	dmg = sqrtf(dps) * std::pow(dmg, 0.25f) * THREAT_MOD;
	power = dmg * sqrtf(def->GetHealth() + maxShield * 2.0f);
}

CCircuitDef::~CCircuitDef()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
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
		STerrainMapImmobileType& it = terrainData.areaData0.immobileType[immobileTypeId];
		isAmphibious = ((it.minElevation < -SQUARE_SIZE * 5) || (it.maxElevation < SQUARE_SIZE * 5)) && !IsFloater();
	}
	isLander = !IsFloater() && !IsAbleToFly() && !IsAmphibious() && !IsSubmarine();
}

CCircuitDef& CCircuitDef::operator++()
{
	++count;
	return *this;
}

// FIXME: ~CCircuitDef should fail with delete
//CCircuitDef CCircuitDef::operator++(int)
//{
//	CCircuitDef temp = *this;
//	count++;
//	return temp;
//}

CCircuitDef& CCircuitDef::operator--()
{
	--count;
	return *this;
}

//CCircuitDef CCircuitDef::operator--(int)
//{
//	CCircuitDef temp = *this;
//	count--;
//	return temp;
//}

} // namespace circuit
