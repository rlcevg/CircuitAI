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
using namespace terrain;

#define THREAT_MOD		(1.0f / 128.0f)

CCircuitDef::RoleName* CCircuitDef::roleNames;
CCircuitDef::AttrName* CCircuitDef::attrNames;
CCircuitDef::FireName CCircuitDef::fireNames = {
	{"hold",   CCircuitDef::FireType::HOLD},
	{"return", CCircuitDef::FireType::RETURN},
	{"open",   CCircuitDef::FireType::OPEN},
};

void CCircuitDef::InitStatic(CCircuitAI* circuit, CMaskHandler* roleMasker, CMaskHandler* attrMasker)
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

	std::vector<std::pair<std::string, CMaskHandler::TypeMask>> attrs = {
		{"",          {ATTR_TYPE(NONE),      CCircuitDef::RoleMask::NONE}},
		{"melee",     {ATTR_TYPE(MELEE),     CCircuitDef::AttrMask::MELEE}},
		{"boost",     {ATTR_TYPE(BOOST),     CCircuitDef::AttrMask::BOOST}},
		{"no_jump",   {ATTR_TYPE(NO_JUMP),   CCircuitDef::AttrMask::NO_JUMP}},
		{"no_strafe", {ATTR_TYPE(NO_STRAFE), CCircuitDef::AttrMask::NO_STRAFE}},
		{"stockpile", {ATTR_TYPE(STOCK),     CCircuitDef::AttrMask::STOCK}},
		{"siege",     {ATTR_TYPE(SIEGE),     CCircuitDef::AttrMask::SIEGE}},
		{"ret_hold",  {ATTR_TYPE(RET_HOLD),  CCircuitDef::AttrMask::RET_HOLD}},
		{"ret_fight", {ATTR_TYPE(RET_FIGHT), CCircuitDef::AttrMask::RET_FIGHT}},
		{"solo",      {ATTR_TYPE(SOLO),      CCircuitDef::AttrMask::SOLO}},
		{"base",      {ATTR_TYPE(BASE),      CCircuitDef::AttrMask::BASE}},
		{"dg_cost",   {ATTR_TYPE(DG_COST),   CCircuitDef::AttrMask::DG_COST}},
		{"dg_still",  {ATTR_TYPE(DG_STILL),  CCircuitDef::AttrMask::DG_STILL}},
		{"jump",      {ATTR_TYPE(JUMP),      CCircuitDef::AttrMask::JUMP}},
		{"onoff",     {ATTR_TYPE(ONOFF),     CCircuitDef::AttrMask::ONOFF}},
		{"vampire",   {ATTR_TYPE(VAMPIRE),   CCircuitDef::AttrMask::VAMPIRE}},
		{"rare",      {ATTR_TYPE(RARE),      CCircuitDef::AttrMask::RARE}},
		{"fence",     {ATTR_TYPE(FENCE),     CCircuitDef::AttrMask::FENCE}},
	};
	for (auto& kv : attrs) {
		CMaskHandler::TypeMask tm = attrMasker->GetTypeMask(kv.first);
		if ((tm.type != kv.second.type) || (tm.mask != kv.second.mask)) {
			circuit->LOG("AttrError: %s = (%i, 0x%08X) != (%i, 0x%08X)", kv.first.c_str(),
						 kv.second.type, kv.second.mask, tm.type, tm.mask);
		}
	}
	CCircuitDef::attrNames = &attrMasker->GetMasks();
}

CCircuitDef::CCircuitDef(CCircuitAI* circuit, UnitDef* def, std::unordered_set<Id>& buildOpts,
						 Resource* resM, Resource* resE, const SArmorInfo& armor)
		: def(def)
		, mainRole(ROLE_TYPE(ASSAULT))
		, enemyRole(RoleMask::NONE)
		, respRole(RoleMask::NONE)
		, role(RoleMask::NONE)
		, attr(NONE)
		, buildOptions(buildOpts)
		, goalBuildMod(-1.f)
		, count(0)
		, buildCounts(0)
		, sinceFrame(-1)
		, cooldown(0)
		, dgunDef(nullptr)
		, weaponDef(nullptr)
		, dgunMount(nullptr)
		, shieldMount(nullptr)
		, weaponMount(nullptr)
		, pwrDmg(.0f)
		, defDmg(.0f)
		, airThrDmg(.0f)
		, surfThrDmg(.0f)
		, waterThrDmg(.0f)
		, aoe(.0f)
		, power(.0f)
		, defThreat(.0f)
		, minRange(.0f)
		, maxRangeType(RangeType::AIR)
		, shieldRadius(.0f)
		, maxShield(.0f)
		, reloadTime(0)
		, targetCategory(0)
		, targetCategoryDGun(0)
		, immobileTypeId(-1)
		, mobileTypeId(-1)
		, isIgnore(false)
		, isAttacker(false)
		, isAlwaysHit(false)
		, hasSurfToAir(false)
		, hasSurfToLand(false)
		, hasSurfToWater(false)
		, hasSubToAir(false)
		, hasSubToLand(false)
		, hasSubToWater(false)
		, hasSurfToAirDGun(false)
		, hasSurfToLandDGun(false)
		, hasSurfToWaterDGun(false)
		, hasSubToAirDGun(false)
		, hasSubToLandDGun(false)
		, hasSubToWaterDGun(false)
		, isFloater(false)
		, isAmphibious(false)
		, isLander(false)
		, isSurfer(false)
		, isAbleToSwim(false)
		, isAbleToDive(false)
		, isMex(false)
		, isWind(false)
		, isPylon(false)
		, isAssist(false)
		, isRadar(false)
		, isSonar(false)
		, isDecoy(false)
		, isOnSlow(false)
		, isOn(true)
		, jumpRange(.0f)
		, retreat(-1.f)
		, radius(-1.f)
		, height(-1.f)
		, topOffset(-1.f)
{
	thrDmgMod.fill(1.f);
	maxRange.fill(.0f);
	threatRange.fill(0);

	id = def->GetUnitDefId();

	buildDistance  = def->GetBuildDistance();
	buildSpeed     = workerTime = def->GetBuildSpeed();
	selfDCountdown = def->GetSelfDCountdown();
	maxThisUnit    = def->GetMaxThisUnit();

//	maxRange[static_cast<RangeT>(RangeType::MAX)] = def->GetMaxWeaponRange();
	hasDGun         = def->CanManualFire();
	category        = def->GetCategory();
	noChaseCategory = (def->GetNoChaseCategory() | circuit->GetBadCategory())
					  & ~circuit->GetGoodCategory();

	const int ft = def->GetFireState();
	fireState = (ft < 0) ? FireType::OPEN : static_cast<FireType>(ft);

	health       = def->GetHealth();
	speed        = def->GetSpeed();  // elmos per second
	losRadius    = def->GetLosRadius();
	sonarRadius  = def->GetSonarRadius();
	costM        = def->GetCost(resM);
	costE        = def->GetCost(resE);
	upkeepM      = def->GetUpkeep(resM);
	upkeepE      = def->GetUpkeep(resE);
	extractsM    = def->GetExtractsResource(resM);  // FIXME: ZK = 0.001f
	extrRangeM   = def->GetResourceExtractorRange(resM);
	cloakCost    = std::max(def->GetCloakCost(), def->GetCloakCostMoving());
	buildTime    = def->GetBuildTime();
	captureSpeed = def->IsAbleToCapture() ? def->GetCaptureSpeed() / TEAM_SLOWUPDATE_RATE : 0.f;
//	altitude     = def->GetWantedHeight();

	COOAICallback* clb = circuit->GetCallback();
	MoveData* md = def->GetMoveData();
	isSubmarine  = (md == nullptr) ? false : md->IsSubMarine();
	delete md;
	isAbleToFly       = def->IsAbleToFly();
	isPlane           = !def->IsHoverAttack() && isAbleToFly;
	isStealth         = def->IsStealth();
	isSonarStealth    = def->IsSonarStealth();
	isTurnLarge       = (speed / (def->GetTurnRate() + DIV0_SLACK) > 0.09f);  // empirical magic number
	isAbleToCloak     = def->IsAbleToCloak();
	isAbleToRepair    = def->IsAbleToRepair();
	isAbleToReclaim   = def->IsAbleToReclaim();
	isAbleToResurrect = def->IsAbleToResurrect();
	// Factory: def->IsBuilder() && !def->GetYardMap(0).empty() && !def->GetBuildOptions().empty()
	isAbleToAssist    = def->IsAbleToAssist() && !clb->UnitDef_HasYardMap(id);
	isAbleToRestore   = def->IsAbleToRestore();
	isReclaimable     = def->IsReclaimable();
	isCapturable      = def->IsCapturable();

	const std::map<std::string, std::string>& customParams = def->GetCustomParams();
	auto it = customParams.find("energyconv_capacity");
	if (it != customParams.end()) {
		upkeepE += utils::string_to_float(it->second);
	}

	it = customParams.find("canjump");
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
	} else if (customParams.find("iscommander") != customParams.end()) {
		AddRole(ROLE_TYPE(COMM));
	}

	it = customParams.find("midposoffset");
	if (it != customParams.end()) {
		const std::string& str = it->second;
		std::string::const_iterator start = str.begin();
		std::string::const_iterator end = str.end();
		std::regex pattern("([+-]?([0-9]*[.])?[0-9]+)");
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
		AddAttribute(ATTR_TYPE(STOCK));
		weaponDef = circuit->GetWeaponDef(stockDef->GetWeaponDefId());
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
	float airDps = .0f;
	float airDmg = .0f;
	float surfDps = .0f;
	float surfDmg = .0f;
	float waterDps = .0f;
	float waterDmg = .0f;
	CWeaponDef* bestDGunDef = nullptr;
	CWeaponDef* bestWpDef = nullptr;
	WeaponMount* bestDGunMnt = nullptr;
	WeaponMount* bestWpMnt = nullptr;
	bool canSurfTargetAir = false;
	bool canSurfTargetLand = false;
	bool canSurfTargetWater = false;
	bool canSubTargetAir = false;
	bool canSubTargetLand = false;
	bool canSubTargetWater = false;
	auto mounts = def->GetWeaponMounts();
	for (WeaponMount* mount : mounts) {
		WeaponDef* wd = mount->GetWeaponDef();
		const std::map<std::string, std::string>& customParams = wd->GetCustomParams();

		if (customParams.find("fake_weapon") != customParams.end()) {
			delete wd;
			delete mount;
			continue;
		}

		float dps = .0f;
		float dmg = .0f;

		float scale = wd->IsParalyzer() ? 0.5f : 1.0f;
		auto it = customParams.find("disarmdamageonly");
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

		float extraDmg = .0f;
		it = customParams.find("extra_damage");
		if (it != customParams.end()) {
			extraDmg += utils::string_to_float(it->second);
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

		int weaponCat = mount->GetOnlyTargetCategory();
		targetCategory |= weaponCat;

		aoe = std::max(aoe, wd->GetAreaOfEffect());

		std::string wt(wd->GetType());  // @see https://springrts.com/wiki/Gamedev:WeaponDefs
		const float projectileSpeed = wd->GetProjectileSpeed();
		const float range = wd->GetRange();

		isAlwaysHit |= ((wt == "Cannon") || (wt == "DGun") || (wt == "EmgCannon") || (wt == "Flame") ||
				(wt == "LaserCannon") || (wt == "AircraftBomb")) && (projectileSpeed * FRAMES_PER_SEC >= .8f * range);  // Cannons with fast projectiles
		isAlwaysHit |= (wt == "BeamLaser") || (wt == "LightningCannon") || (wt == "Rifle") ||  // Instant-hit
				(((wt == "MissileLauncher") || (wt == "StarburstLauncher") || ((wt == "TorpedoLauncher") && wd->IsSubMissile())) && wd->IsTracks());  // Missiles
		const bool isAirWeapon = isAlwaysHit && (range > 150.f);
		canSurfTargetAir |= isAirWeapon;

		const bool isLandWeapon = ((wt != "TorpedoLauncher") || wd->IsSubMissile());
		canSurfTargetLand |= isLandWeapon;

		const bool isWaterWeapon = wd->IsWaterWeapon();
		canSurfTargetWater |= isWaterWeapon;

		const bool isFireSubmersed = wd->IsFireSubmersed();
		canSubTargetAir |= (isAirWeapon && isWaterWeapon && isFireSubmersed);
		canSubTargetLand |= (isLandWeapon && isWaterWeapon && isFireSubmersed);
		canSubTargetWater |= (isWaterWeapon && isFireSubmersed);

		it = customParams.find("statsdamage");
		if (it != customParams.end()) {
			float localDmg = utils::string_to_float(it->second);
			localDmg *= std::pow(2.0f, (wd->IsDynDamageInverted() ? 1 : -1) * wd->GetDynDamageExp());
			dmg += localDmg;
			dps += localDmg * wd->GetSalvoSize() / reloadTime * scale;
		} else {
			Damage* damage = wd->GetDamage();
			const std::vector<float>& damages = damage->GetTypes();
			delete damage;
			auto adjustDamage = [&damages, wd, reloadTime, scale](const std::vector<int>& armorTypes, float& outDmg, float& outDps) {
				float localDmg = .0f;
				for (int type : armorTypes) {
					localDmg += damages[type];
				}
				localDmg /= armorTypes.size();
				localDmg *= std::pow(2.0f, (wd->IsDynDamageInverted() ? 1 : -1) * wd->GetDynDamageExp());
				outDmg += localDmg;
				outDps += localDmg * wd->GetSalvoSize() / reloadTime * scale;
			};
			if ((weaponCat & circuit->GetAirCategory()) && isAirWeapon) {
				adjustDamage(armor.airTypes, airDmg, airDps);
			}
			if ((weaponCat & circuit->GetLandCategory()) && isLandWeapon) {
				adjustDamage(armor.surfTypes, surfDmg, surfDps);
			}
			if ((weaponCat & circuit->GetWaterCategory()) && isWaterWeapon) {
				adjustDamage(armor.waterTypes, waterDmg, waterDps);
			}
		}
		airDmg += dmg;
		airDps += dps;
		surfDmg += dmg;
		surfDps += dps;
		waterDmg += dmg;
		waterDps += dps;

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

		// FIXME: hasDGun is actually "manual command to fire required". There's "DGun" weapon type,
		//        but game can make custom weapons and commands and there's no way to tell that it's
		//        secondary manually fired weapon. @see ZK's Krow
		if (hasDGun && wd->IsManualFire() && (reloadTime < bestDGunReload)) {
			// NOTE: Disable commander's dgun, because no usage atm
//			if (customParams.find("manualfire") == customParams.end()) {
				bestDGunReload = reloadTime;
				bestDGunDef = circuit->GetWeaponDef(wd->GetWeaponDefId());
				delete bestDGunMnt;
				bestDGunMnt = mount;
				targetCategoryDGun |= weaponCat;
				hasSurfToAirDGun   |= (weaponCat & circuit->GetAirCategory()) && isAirWeapon;
				hasSurfToLandDGun  |= (weaponCat & circuit->GetLandCategory()) && isLandWeapon;
				hasSurfToWaterDGun |= (weaponCat & circuit->GetWaterCategory()) && isWaterWeapon;
				hasSubToAirDGun    |= (weaponCat & circuit->GetAirCategory()) && (isAirWeapon && isWaterWeapon && isFireSubmersed);
				hasSubToLandDGun   |= (weaponCat & circuit->GetLandCategory()) && (isLandWeapon && isWaterWeapon && isFireSubmersed);
				hasSubToWaterDGun  |= (weaponCat & circuit->GetWaterCategory()) && (isWaterWeapon && isFireSubmersed);
//			} else {  // FIXME: Dynamo com workaround
//				delete mount;
//			}
		} else if (wd->IsShield()) {
			if (shieldMount == nullptr) {
				shieldMount = mount;  // NOTE: Unit may have more than 1 shield
			} else {
				delete mount;
			}
		} else if (range < bestWpRange) {
			bestWpDef = circuit->GetWeaponDef(wd->GetWeaponDefId());
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
		airDps /= mounts.size();
		airDmg /= mounts.size();
		surfDps /= mounts.size();
		surfDmg /= mounts.size();
		waterDps /= mounts.size();
		waterDmg /= mounts.size();
	}

	if (minReloadTime < std::numeric_limits<float>::max()) {
		reloadTime = minReloadTime * FRAMES_PER_SEC;
	}
	if (bestDGunReload < std::numeric_limits<float>::max()) {
		dgunDef = bestDGunDef;
		dgunMount = bestDGunMnt;
	}
	if (bestWpRange < std::numeric_limits<float>::max()) {
		weaponDef = bestWpDef;
		weaponMount = bestWpMnt;
	}

	isAttacker = (airDps > .1f) || (surfDps > .1f) || (waterDps > .1f);
	if (IsMobile() && !IsAttacker()) {  // mobile bomb?
		WeaponDef* wd = def->GetDeathExplosion();
		aoe = wd->GetAreaOfEffect();
		if (aoe > 64.0f) {
			// power
			float localDmg = .0f;
			it = customParams.find("statsdamage");
			if (it != customParams.end()) {
				localDmg = utils::string_to_float(it->second);
			} else {
				Damage* damage = wd->GetDamage();
				const std::vector<float>& damages = damage->GetTypes();
				delete damage;
				localDmg = damages[0];  // default
			}
			float localDps = localDmg * wd->GetSalvoSize();
			airDmg += localDmg;
			airDps = localDps;
			surfDmg += localDmg;
			surfDps = localDps;
			waterDmg += localDmg;
			waterDps = localDps;
			isAttacker = (airDps > .1f) || (surfDps > .1f) || (waterDps > .1f);
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
	hasSurfToAir   = (targetCategory & circuit->GetAirCategory()) && canSurfTargetAir;
	hasSurfToLand  = (targetCategory & circuit->GetLandCategory()) && canSurfTargetLand;
	hasSurfToWater = (targetCategory & circuit->GetWaterCategory()) && canSurfTargetWater;
	hasSubToAir    = (targetCategory & circuit->GetAirCategory()) && canSubTargetAir;
	hasSubToLand   = (targetCategory & circuit->GetLandCategory()) && canSubTargetLand;
	hasSubToWater  = (targetCategory & circuit->GetWaterCategory()) && canSubTargetWater;

	// TODO: Include projectile-speed/range, armor
	//       health /= def->GetArmoredMultiple();
	float dmg = std::max(std::max(waterDmg, surfDmg), airDmg);
	float dps = std::max(std::max(waterDps, surfDps), airDps);
	defDmg = pwrDmg = sqrtf(dps) * std::pow(dmg, 0.25f) * THREAT_MOD;
	defThreat = power = defDmg * sqrtf(health + maxShield * SHIELD_MOD);
	airThrDmg = sqrtf(airDps) * std::pow(airDmg, 0.25f) * THREAT_MOD;
	surfThrDmg = sqrtf(surfDps) * std::pow(surfDmg, 0.25f) * THREAT_MOD;
	waterThrDmg = sqrtf(waterDps) * std::pow(waterDmg, 0.25f) * THREAT_MOD;
#if 0
	circuit->LOG("THREAT %s | def=%f air=%f surf=%f water=%f", def->GetName(), defThreat,
			airThrDmg * sqrtf(health + maxShield * SHIELD_MOD),
			surfThrDmg * sqrtf(health + maxShield * SHIELD_MOD),
			waterThrDmg * sqrtf(health + maxShield * SHIELD_MOD));
#endif
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
		std::map<SMobileType::Id, float> mtUsability;
		for (CCircuitDef::Id buildId : GetBuildOptions()) {
			CCircuitDef* bdef = circuit->GetCircuitDef(buildId);
			if ((bdef == nullptr) || !bdef->IsMobile() || !bdef->IsAttacker()) {
				continue;
			}
			SMobileType::Id mtId = terrainData.udMobileType[bdef->GetId()];
			if ((mtId < 0) || (mtUsability.find(mtId) != mtUsability.end())) {
				continue;
			}
			SMobileType& mt = terrainData.areaData0.mobileType[mtId];
			mtUsability[mtId] = mt.area.empty() ? 00.0 : mt.areaLargest->percentOfMap;
		}
		float useMost = .0f;
		SMobileType::Id mtId = mobileTypeId;  // -1
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

	auto fillSurface = [this, circuit](const float minElev, const float maxElev) {
		const bool isOnWater = def->IsFloater() && !IsSubmarine() && !IsAbleToFly();
		isFloater = isOnWater && ((minElev < -1.f) && (maxElev < 1.f));
		isLander = (minElev > -100.f) && (maxElev > 100.f);
		isSurfer = isOnWater && ((minElev < -1.f) && (maxElev > 100.f));
		// FIXME: can't filter out hover. This should be floater or walking-ship
		isAbleToSwim = IsFloater();  // isOnWater && ((minElev < -1.f) && (maxElev > -100.f));
		isAbleToDive = ((minElev < -1.f) || (maxElev < 1.f)) && !IsFloater() && !IsSurfer() && !IsLander();
	};
	if (IsMobile()) {
		if (mobileTypeId >= 0) {
			SMobileType& mt = terrainData.areaData0.mobileType[mobileTypeId];
			fillSurface(mt.minElevation, mt.maxElevation);
		}
	} else {
		if (immobileTypeId >= 0) {
			SImmobileType& it = terrainData.areaData0.immobileType[immobileTypeId];
			fillSurface(it.minElevation, it.maxElevation);
		}
	}
	isAmphibious = IsAbleToDive() && !IsSubmarine() && !IsSurfer();

	UnitDef* decoyDef = def->GetDecoyDef();
	if (decoyDef != nullptr) {
		circuit->GetCircuitDef(decoyDef->GetUnitDefId())->SetIsDecoy(true);
		delete decoyDef;
	}

	int surface = 0;
	if (isAbleToFly) surface++;
	if (isFloater) surface++;
	if (isSubmarine) surface++;
	if (isAmphibious) surface++;
	if (isLander) surface++;
	if (isSurfer) surface++;
	if (surface != 1) {
		circuit->LOG("ALARM: %s | isAbleToFly: %i | isFloater: %i | isSubmarine: %i | isAmphibious: %i | isLander: %i | isSurfer: %i", def->GetName(),
				isAbleToFly, isFloater, isSubmarine, isAmphibious, isLander, isSurfer);
	}
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

bool CCircuitDef::IsInWater(float elevation, float posY) {
	GetHeight();
	return (elevation < -height) && (posY < -topOffset);
}

bool CCircuitDef::IsPredictInWater(float elevation)
{
	return IsAbleToDive() ? IsInWater(elevation, elevation) : false;
}

AIFloat3 CCircuitDef::GetMidPosOffset(int facing) const
{
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH:
			return AIFloat3(-midPosOffset.x, 0, -midPosOffset.z);
		case UNIT_FACING_EAST:
			return AIFloat3(-midPosOffset.z, 0, +midPosOffset.x);
		case UNIT_FACING_NORTH:
			return AIFloat3(+midPosOffset.x, 0, +midPosOffset.z);
		case UNIT_FACING_WEST:
			return AIFloat3(+midPosOffset.z, 0, -midPosOffset.x);
	}
}

} // namespace circuit
