/*
 * CircuitDef.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
#define SRC_CIRCUIT_UNIT_CIRCUITDEF_H_

#include "terrain/TerrainData.h"
#include "util/MaskHandler.h"

#include "UnitDef.h"

#include <unordered_set>
#include <array>

namespace springai {
	class WeaponMount;
}

namespace circuit {

#define ROLE_TYPE(x)	static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::x)
#define ATTR_TYPE(x)	static_cast<CCircuitDef::AttrT>(CCircuitDef::AttrType::x)

class CWeaponDef;

class CCircuitDef final {
public:
	friend class CInitScript;

	using Id = int;
	enum class RangeType: char {AIR = 0, LAND = 1, WATER = 2, _SIZE_};
	enum class ThreatType: char {AIR = 0, SURF = 1, WATER = 2, CLOAK = 3, SHIELD = 4, _SIZE_};
	using RangeT = std::underlying_type<RangeType>::type;
	using ThreatT = std::underlying_type<ThreatType>::type;
	using ThrDmgArray = std::array<float, CMaskHandler::GetMaxMasks()>;

	struct SArmorInfo {
		std::vector<int> airTypes;  // air
		std::vector<int> surfTypes;  // surface, even on water
		std::vector<int> waterTypes;  // underwater
	};

	// Not implemented: mine, transport
	// No special task: air, sub, static, heavy, comm
	enum class RoleType: CMaskHandler::Type {NONE = -1,
		BUILDER = 0, SCOUT, RAIDER, RIOT,
		ASSAULT, SKIRM, ARTY, AA,
		AS, AH, BOMBER, SUPPORT,
		MINE, TRANS, AIR, SUB,
		STATIC, HEAVY, SUPER, COMM};
	enum RoleMask: CMaskHandler::Mask {NONE = 0x00000000,
		BUILDER = 0x00000001, SCOUT = 0x00000002, RAIDER = 0x00000004, RIOT    = 0x00000008,
		ASSAULT = 0x00000010, SKIRM = 0x00000020, ARTY   = 0x00000040, AA      = 0x00000080,
		AS      = 0x00000100, AH    = 0x00000200, BOMBER = 0x00000400, SUPPORT = 0x00000800,
		MINE    = 0x00001000, TRANS = 0x00002000, AIR    = 0x00004000, SUB     = 0x00008000,
		STATIC  = 0x00010000, HEAVY = 0x00020000, SUPER  = 0x00040000, COMM    = 0x00080000};
	using RoleT = std::underlying_type<RoleType>::type;
	using RoleM = std::underlying_type<RoleMask>::type;

	/*
	 * MELEE:     always move close to target, disregard attack range
	 * BOOST:     boost speed on retreat
	 * NO_JUMP:   disable jump on retreat
	 * NO_STRAFE: isable gunship's strafe
	 * STOCK:     stockpile weapon before any task (Not implemented)
	 * SIEGE:     mobile units use Fight instead of Move; arty ignores siege buildings
	 * RET_HOLD:  hold fire on retreat
	 * RET_FIGHT: fight on retreat
	 * SOLO:      construction initiator, won't join task if already started
	 * BASE:      base builder, high priority for energy and storage tasks
	 * VAMPIRE:   reclaim enemy units without threat check
	 * ONOFF:     toggle weapon state, for mobile targets - state1, for static - state2
	 * RARE:      build unit from T1 factory even when T2+ factory is available
	 */
	enum class AttrType: RoleT {NONE = -1,
		MELEE = 0, BOOST, NO_JUMP, NO_STRAFE,
		STOCK, SIEGE, RET_HOLD, RET_FIGHT,
		SOLO, BASE, VAMPIRE, ONOFF, RARE, _SIZE_};
	enum AttrMask: RoleM {
		MELEE = 0x00000001, BOOST = 0x00000002, NO_JUMP  = 0x00000004, NO_STRAFE = 0x00000008,
		STOCK = 0x00000010, SIEGE = 0x00000020, RET_HOLD = 0x00000040, RET_FIGHT = 0x00000080,
		SOLO  = 0x00000100, BASE  = 0x00000200, VAMPIRE  = 0x00000400, ONOFF     = 0x00000800,
		RARE  = 0x00001000};
	using AttrT = std::underlying_type<AttrType>::type;
	using AttrM = std::underlying_type<AttrMask>::type;

	enum FireType: int {HOLD = 0, RETURN = 1, OPEN = 2, _SIZE_};

	static RoleM GetMask(RoleT type) { return CMaskHandler::GetMask(type); }

	using RoleName = const CMaskHandler::MaskName;
	using AttrName = const CMaskHandler::MaskName;
	using FireName = std::map<std::string, FireType>;
	static RoleName& GetRoleNames() { return *roleNames; }
	static AttrName& GetAttrNames() { return *attrNames; }
	static FireName& GetFireNames() { return fireNames; }

	static void InitStatic(CCircuitAI* circuit, CMaskHandler* roleMasker, CMaskHandler* attrMasker);

//	CCircuitDef(const CCircuitDef& that) = delete;
	CCircuitDef& operator=(const CCircuitDef&) = delete;
	CCircuitDef(CCircuitAI* circuit, springai::UnitDef* def, std::unordered_set<Id>& buildOpts,
			springai::Resource* resM, springai::Resource* resE, const SArmorInfo& armor);
	~CCircuitDef();

	void Init(CCircuitAI* circuit);

	Id GetId() const { return id; }
	springai::UnitDef* GetDef() const { return def; }

	void SetMainRole(RoleT type) { mainRole = type; }
	RoleT GetMainRole() const { return mainRole; }
	void AddEnemyRole(RoleT type) { enemyRole |= GetMask(type); }
	void AddEnemyRoles(RoleM mask) { enemyRole |= mask; }
	bool IsEnemyRoleAny(RoleM value) const { return (enemyRole & value) != 0; }

	AttrM GetAttributes() const { return attr; }
	void AddAttribute(AttrT type) { attr |= GetMask(type); }
	void DelAttribute(AttrT type) { attr &= ~GetMask(type); }
	void TglAttribute(AttrT type) { attr ^= GetMask(type); }
	bool IsAttrAny(AttrM value) const { return (attr & value) != 0; }
	void AddRole(RoleT type) { AddRole(type, type); }
	void AddRole(RoleT type, RoleT bindType);
	bool IsRespRoleAny(RoleM value) const { return (respRole & value) != 0; }
	bool IsRoleAny(RoleM value)     const { return (role & value) != 0; }
//	bool IsRoleEqual(RoleM value)   const { return role == value; }
//	bool IsRoleContain(RoleM value) const { return (role & value) == value; }

	bool IsRoleBuilder()  const { return role & RoleMask::BUILDER; }
	bool IsRoleScout()    const { return role & RoleMask::SCOUT; }
	bool IsRoleRaider()   const { return role & RoleMask::RAIDER; }
	bool IsRoleRiot()     const { return role & RoleMask::RIOT; }
	bool IsRoleAssault()  const { return role & RoleMask::ASSAULT; }
	bool IsRoleSkirm()    const { return role & RoleMask::SKIRM; }
	bool IsRoleArty()     const { return role & RoleMask::ARTY; }
	bool IsRoleAA()       const { return role & RoleMask::AA; }
	bool IsRoleAS()       const { return role & RoleMask::AS; }
	bool IsRoleAH()       const { return role & RoleMask::AH; }
	bool IsRoleBomber()   const { return role & RoleMask::BOMBER; }
	bool IsRoleSupport()  const { return role & RoleMask::SUPPORT; }
	bool IsRoleMine()     const { return role & RoleMask::MINE; }
	bool IsRoleTrans()    const { return role & RoleMask::TRANS; }
	bool IsRoleAir()      const { return role & RoleMask::AIR; }
	bool IsRoleSub()      const { return role & RoleMask::SUB; }
	bool IsRoleStatic()   const { return role & RoleMask::STATIC; }
	bool IsRoleHeavy()    const { return role & RoleMask::HEAVY; }
	bool IsRoleSuper()    const { return role & RoleMask::SUPER; }
	bool IsRoleComm()     const { return role & RoleMask::COMM; }

	bool IsAttrMelee()    const { return attr & AttrMask::MELEE; }
	bool IsAttrBoost()    const { return attr & AttrMask::BOOST; }
	bool IsAttrNoJump()   const { return attr & AttrMask::NO_JUMP; }
	bool IsAttrNoStrafe() const { return attr & AttrMask::NO_STRAFE; }
	bool IsAttrStock()    const { return attr & AttrMask::STOCK; }
	bool IsAttrSiege()    const { return attr & AttrMask::SIEGE; }
	bool IsAttrRetHold()  const { return attr & AttrMask::RET_HOLD; }
	bool IsAttrRetFight() const { return attr & AttrMask::RET_FIGHT; }
//	bool IsAttrSolo()     const { return attr & AttrMask::SOLO; }  // per-unit
//	bool IsAttrBase()     const { return attr & AttrMask::BASE; }  // per-unit
	bool IsAttrVampire()  const { return attr & AttrMask::VAMPIRE; }
	bool IsAttrOnOff()    const { return attr & AttrMask::ONOFF; }
	bool IsAttrRare()     const { return attr & AttrMask::RARE; }

	bool IsHoldFire()   const { return fireState == FireType::HOLD; }
	bool IsReturnFire() const { return fireState == FireType::RETURN; }
	bool IsOpenFire()   const { return fireState == FireType::OPEN; }

	const std::unordered_set<Id>& GetBuildOptions() const { return buildOptions; }
	float GetBuildDistance() const { return buildDistance; }
	float GetBuildSpeed() const { return buildSpeed; }
	void SetBuildSpeed(float value) { buildSpeed = value; }
	float GetWorkerTime() const { return workerTime; }
	void SetGoalBuildMod(float value) { goalBuildMod = value; }
	float GetGoalBuildMod() const { return goalBuildMod; }
	float GetGoalBuildTime(const float metalIncome) const { return goalBuildMod / (metalIncome + DIV0_SLACK); }
	inline bool CanBuild(const Id buildDefId) const {	return buildOptions.find(buildDefId) != buildOptions.end(); }
	inline bool CanBuild(const CCircuitDef* buildDef) const { return CanBuild(buildDef->GetId()); }
	int GetCount() const { return count; }

	void Inc() { ++count; }
	void Dec() { --count; }

	CCircuitDef& operator++();     // prefix  (++C): no parameter, returns a reference
//	CCircuitDef  operator++(int);  // postfix (C++): dummy parameter, returns a value
	CCircuitDef& operator--();     // prefix  (--C): no parameter, returns a reference
//	CCircuitDef  operator--(int);  // postfix (C--): dummy parameter, returns a value
	bool operator==(const CCircuitDef& rhs) { return id == rhs.id; }
	bool operator!=(const CCircuitDef& rhs) { return id != rhs.id; }

	int GetSelfDCountdown() const { return selfDCountdown; }
	void SetMaxThisUnit(int value) { maxThisUnit = value; }
	int GetMaxThisUnit() const { return maxThisUnit; }
	void SetSinceFrame(int frame) { sinceFrame = frame; }
	void SetCooldown(int interval) { cooldown = interval; }
	void AdjustSinceFrame(int frame) { sinceFrame = frame + cooldown; }
	bool IsAvailable() const { return maxThisUnit > count; }
	bool IsAvailable(int frame) const { return IsAvailable() && (frame >= sinceFrame); }

	void IncBuild() { ++buildCounts; }
	void DecBuild() { --buildCounts; }
	int GetBuildCount() const { return buildCounts; }

	bool HasDGun() const { return hasDGun; }
	CWeaponDef* GetDGunDef() const { return dgunDef; }
	CWeaponDef* GetWeaponDef() const { return weaponDef; }
	springai::WeaponMount* GetDGunMount() const { return dgunMount; }
	springai::WeaponMount* GetShieldMount() const { return shieldMount; }
	springai::WeaponMount* GetWeaponMount() const { return weaponMount; }
	float GetPwrDamage() const { return pwrDmg; }  // ally
	float GetDefDamage() const { return defDmg; }  // enemy, for influence
	float GetAirDmg(RoleT type) const { return airThrDmg * thrDmgMod[type]; }  // enemy
	float GetSurfDmg(RoleT type) const { return surfThrDmg * thrDmgMod[type]; }  // enemy
	float GetWaterDmg(RoleT type) const { return waterThrDmg * thrDmgMod[type]; }  // enemy
	float GetAoe() const { return aoe; }
	float GetPower() const { return power; }
	float GetDefThreat() const { return defThreat; }
	float GetMinRange() const { return minRange; }
	float GetMaxRange(RangeType type) const { return maxRange[static_cast<RangeT>(type)]; }
	float GetMaxRange() const { return maxRange[static_cast<RangeT>(maxRangeType)]; }
	int GetThreatRange(ThreatType type) const { return threatRange[static_cast<ThreatT>(type)]; }
	float GetShieldRadius() const { return shieldRadius; }
	float GetMaxShield() const { return maxShield; }
	int GetFireState() const { return fireState; }
	int GetReloadTime() const { return reloadTime; }
	int GetCategory() const { return category; }
	int GetTargetCategory() const { return targetCategory; }
	int GetTargetCategoryDGun() const { return targetCategoryDGun; }
	int GetNoChaseCategory() const { return noChaseCategory; }

	void ModPower(float mod) { pwrDmg *= mod; power *= mod; }
	void ModDefThreat(float mod) { defDmg *= mod; defThreat *= mod; }
	void ModThreatMod(RoleT type, float mod) { thrDmgMod[type] *= mod; }
	void ModAirThreat(float mod) { airThrDmg *= mod; }
	void ModSurfThreat(float mod) { surfThrDmg *= mod; }
	void ModWaterThreat(float mod) { waterThrDmg *= mod; }
	void SetThreatRange(ThreatType type, int range) { threatRange[static_cast<ThreatT>(type)] = range; }
	void SetFireState(FireType ft) { fireState = ft; }
	void SetReloadTime(int time) { reloadTime = time; }

	terrain::SImmobileType::Id GetImmobileId() const { return immobileTypeId; }
	terrain::SMobileType::Id GetMobileId() const { return mobileTypeId; }

	void SetIgnore(bool value) { isIgnore = value; }
	bool IsIgnore() const { return isIgnore; }

	bool IsAttacker()  const { return isAttacker; }
	bool IsAlwaysHit() const { return isAlwaysHit; }
	bool HasSurfToAir()   const { return hasSurfToAir; }
	bool HasSurfToLand()  const { return hasSurfToLand; }
	bool HasSurfToWater() const { return hasSurfToWater; }
	bool HasSubToAir()    const { return hasSubToAir; }
	bool HasSubToLand()   const { return hasSubToLand; }
	bool HasSubToWater()  const { return hasSubToWater; }
	bool HasSurfToAirDGun()   const { return hasSurfToAirDGun; }
	bool HasSurfToLandDGun()  const { return hasSurfToLandDGun; }
	bool HasSurfToWaterDGun() const { return hasSurfToWaterDGun; }
	bool HasSubToAirDGun()    const { return hasSubToAirDGun; }
	bool HasSubToLandDGun()   const { return hasSubToLandDGun; }
	bool HasSubToWaterDGun()  const { return hasSubToWaterDGun; }

	bool IsMobile()          const { return speed > .1f; }
	bool IsPlane()           const { return isPlane; }
	bool IsFloater()         const { return isFloater; }
	bool IsSubmarine()       const { return isSubmarine; }
	bool IsAmphibious()      const { return isAmphibious; }
	bool IsLander()          const { return isLander; }
	bool IsSurfer()          const { return isSurfer; }
	bool IsStealth()         const { return isStealth; }
	bool IsSonarStealth()    const { return isSonarStealth; }
	bool IsTurnLarge()       const { return isTurnLarge; }
	bool IsAbleToFly()       const { return isAbleToFly; }
	bool IsAbleToSwim()      const { return isAbleToSwim; }
	bool IsAbleToDive()      const { return isAbleToDive; }  // for any aircraft = false
	bool IsAbleToCloak()     const { return isAbleToCloak; }
	bool IsAbleToJump()      const { return isAbleToJump; }
	bool IsAbleToRepair()    const { return isAbleToRepair; }
	bool IsAbleToReclaim()   const { return isAbleToReclaim; }
	bool IsAbleToResurrect() const { return isAbleToResurrect; }
	bool IsAbleToAssist()    const { return isAbleToAssist; }
	bool IsAbleToRestore()   const { return isAbleToRestore; }
	bool IsAbleToCapture()   const { return captureSpeed > .1f; }
	bool IsAssistable()      const { return buildTime < 1e6f; }
	bool IsReclaimable()     const { return isReclaimable; }
	bool IsCapturable()      const { return isCapturable; }
	bool IsBuilder()         const { return !buildOptions.empty(); }

	void SetIsMex(bool value) { isMex = value; }
	bool IsMex() const { return isMex; }
	void SetIsWind(bool value) { isWind = value; }
	bool IsWind() const { return isWind; }
	void SetIsPylon(bool value) { isPylon = value; }
	bool IsPylon() const { return isPylon; }
	void SetIsAssist(bool value) { isAssist = value; }
	bool IsAssist() const { return isAssist; }
	void SetIsRadar(bool value) { isRadar = value; }
	bool IsRadar() const { return isRadar; }
	void SetIsSonar(bool value) { isSonar = value ; }
	bool IsSonar() const { return isSonar; }
	void SetIsDecoy(bool value) { isDecoy = value; }
	bool IsDecoy() const { return isDecoy; }
	void SetOnSlow(bool value) { isOnSlow = value; }
	bool IsOnSlow() const { return isOnSlow; }
	void SetOn(bool value) { isOn = value; }
	bool IsOn() const { return isOn; }

	float GetHealth()       const { return health; }
	float GetSpeed()        const { return speed; }
	float GetLosRadius()    const { return losRadius; }
	float GetSonarRadius()  const { return sonarRadius; }
	float GetCostM()        const { return costM; }
	float GetCostE()        const { return costE; }
	float GetUpkeepM()      const { return upkeepM; }
	float GetUpkeepE()      const { return upkeepE; }
	float GetExtractsM()    const { return extractsM; }
	float GetExtrRangeM()   const { return extrRangeM; }
	float GetCloakCost()    const { return cloakCost; }
	float GetBuildTime()    const { return buildTime; }
	float GetCaptureSpeed() const { return captureSpeed; }
//	float GetAltitude()     const { return altitude; }
	float GetJumpRange()    const { return jumpRange; }

	void SetRetreat(float value) { retreat = value; }
	float GetRetreat()   const { return retreat; }

	float GetRadius();
	float GetHeight();
	bool IsInWater(float elevation, float posY);
	bool IsPredictInWater(float elevation);
	bool IsHub() const { return isAbleToAssist && (buildDistance > 200.0f); }

	void SetMidPosOffset(float x, float y, float z) { midPosOffset = springai::AIFloat3(x, y, z); }
	springai::AIFloat3 GetMidPosOffset(int facing) const;

	// script API
	float GetAirThreat() const { return airThrDmg * sqrtf(health + maxShield * SHIELD_MOD); }
	float GetSurfThreat() const { return surfThrDmg * sqrtf(health + maxShield * SHIELD_MOD); }
	float GetWaterThreat() const { return waterThrDmg * sqrtf(health + maxShield * SHIELD_MOD); }

private:
	static RoleName* roleNames;
	static AttrName* attrNames;
	static FireName fireNames;

	// TODO: associate script data with CCircuitDef
	//       instead of using map<Id,data> everywhere
//	asIScriptObject* data;

	Id id;
	springai::UnitDef* def;  // owner
	RoleT mainRole;  // RoleType
	RoleM enemyRole;  // RoleMask
	RoleM respRole;  // unique for response, custom roles, no bindings
	RoleM role;  // implemented roles, no custom roles, only binded
	AttrM attr;
	std::unordered_set<Id> buildOptions;
	float buildDistance;
	float buildSpeed;
	float workerTime;
	float goalBuildMod;
	int count;
	int buildCounts;  // number of builder defs able to build this def;
	int selfDCountdown;
	int maxThisUnit;
	int sinceFrame;
	int cooldown;

	CWeaponDef* dgunDef;
	CWeaponDef* weaponDef;
	springai::WeaponMount* dgunMount;
	springai::WeaponMount* shieldMount;
	springai::WeaponMount* weaponMount;
	float pwrDmg;  // ally damage
	float defDmg;  // enemy damage, for influence
	float airThrDmg;  // air enemy damage
	float surfThrDmg;  // surface, even on water
	float waterThrDmg;  // underwater enemy damage
	ThrDmgArray thrDmgMod;  // mod by role
	float aoe;  // radius
	float power;  // ally max threat
	float defThreat;  // enemy max threat
	float minRange;
	RangeType maxRangeType;
	std::array<float, static_cast<RangeT>(RangeType::_SIZE_)> maxRange;
	std::array<int, static_cast<ThreatT>(ThreatType::_SIZE_)> threatRange;
	float shieldRadius;
	float maxShield;
	FireType fireState;
	int reloadTime;  // frames in ticks
	int category;
	int targetCategory;
	int targetCategoryDGun;
	int noChaseCategory;

	terrain::SImmobileType::Id immobileTypeId;
	terrain::SMobileType::Id   mobileTypeId;

	// ---- Bit fields ---- BEGIN
	bool isIgnore : 1;

	bool isAttacker : 1;
	bool isAlwaysHit : 1;  // FIXME: calc per weapon
	bool hasDGun : 1;

	// TODO: std::bitset<2>
	bool hasSurfToAir : 1;  // air layer
	bool hasSurfToLand : 1;  // surface (water and land)
	bool hasSurfToWater : 1;  // under water
	bool hasSubToAir : 1;
	bool hasSubToLand : 1;
	bool hasSubToWater : 1;
	bool hasSurfToAirDGun : 1;  // air layer
	bool hasSurfToLandDGun : 1;  // surface (water and land)
	bool hasSurfToWaterDGun : 1;  // under water
	bool hasSubToAirDGun : 1;
	bool hasSubToLandDGun : 1;
	bool hasSubToWaterDGun : 1;

	bool isPlane : 1;  // no hover attack
	bool isFloater : 1;
	bool isSubmarine : 1;
	bool isAmphibious : 1;
	bool isLander : 1;
	bool isSurfer : 1;  // isFloater && isLander
	bool isStealth : 1;
	bool isSonarStealth : 1;
	bool isTurnLarge : 1;
	bool isAbleToFly : 1;
	bool isAbleToSwim : 1;  // isFloater && isAmphibious
	bool isAbleToDive : 1;  // isSubmarine || isAmphibious
	bool isAbleToCloak : 1;
	bool isAbleToJump : 1;
	bool isAbleToRepair : 1;
	bool isAbleToReclaim : 1;
	bool isAbleToResurrect : 1;
	bool isAbleToAssist : 1;
	bool isAbleToRestore : 1;
	bool isReclaimable : 1;
	bool isCapturable : 1;

	bool isMex : 1;
	bool isWind : 1;
	bool isPylon : 1;
	bool isAssist : 1;
	bool isRadar : 1;
	bool isSonar : 1;
	bool isDecoy : 1;
	bool isOnSlow : 1;
	bool isOn : 1;
	// ---- Bit fields ---- END

	float health;
	float speed;
	float losRadius;
	float sonarRadius;
	float costM;
	float costE;
	float upkeepM;
	float upkeepE;
	float extractsM;
	float extrRangeM;
	float cloakCost;
	float buildTime;
	float captureSpeed;
//	float altitude;
	float jumpRange;
	float retreat;

	float radius;
	float height;
	float topOffset;  // top point offset in water
	springai::AIFloat3 midPosOffset;
};

inline CCircuitDef& CCircuitDef::operator++()
{
	++count;
	return *this;
}

// FIXME: ~CCircuitDef should fail with delete
//inline CCircuitDef CCircuitDef::operator++(int)
//{
//	CCircuitDef temp = *this;
//	count++;
//	return temp;
//}

inline CCircuitDef& CCircuitDef::operator--()
{
	--count;
	return *this;
}

//inline CCircuitDef CCircuitDef::operator--(int)
//{
//	CCircuitDef temp = *this;
//	count--;
//	return temp;
//}

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
