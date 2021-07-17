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

class CWeaponDef;

class CCircuitDef {
public:
	using Id = int;
	enum class RangeType: char {AIR = 0, LAND = 1, WATER = 2, _SIZE_};
	enum class ThreatType: char {AIR = 0, LAND = 1, WATER = 2, CLOAK = 3, SHIELD = 4, _SIZE_};
	using RangeT = std::underlying_type<RangeType>::type;
	using ThreatT = std::underlying_type<ThreatType>::type;

	// TODO: Rebuild response system on unit vs unit basis (opposed to role vs role).
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
	 */
	enum class AttrType: RoleT {NONE = -1,
		MELEE = 0, BOOST, NO_JUMP, NO_STRAFE,
		STOCK, SIEGE, RET_HOLD, RET_FIGHT, _SIZE_};
	enum AttrMask: RoleM {
		MELEE = 0x00000001, BOOST = 0x00000002, NO_JUMP  = 0x00000004, NO_STRAFE = 0x00000008,
		STOCK = 0x00000010, SIEGE = 0x00000020, RET_HOLD = 0x00000040, RET_FIGHT = 0x00000080};
	using AttrT = std::underlying_type<AttrType>::type;
	using AttrM = std::underlying_type<AttrMask>::type;

	enum FireType: int {HOLD = 0, RETURN = 1, OPEN = 2, _SIZE_};

	static RoleM GetMask(RoleT type) { return CMaskHandler::GetMask(type); }

	using RoleName = const CMaskHandler::MaskName;
	using AttrName = std::map<std::string, AttrType>;
	using FireName = std::map<std::string, FireType>;
	static RoleName& GetRoleNames() { return *roleNames; }
	static AttrName& GetAttrNames() { return attrNames; }
	static FireName& GetFireNames() { return fireNames; }

	static void InitStatic(CCircuitAI* circuit, CMaskHandler* roleMasker);

//	CCircuitDef(const CCircuitDef& that) = delete;
	CCircuitDef& operator=(const CCircuitDef&) = delete;
	CCircuitDef(CCircuitAI* circuit, springai::UnitDef* def, std::unordered_set<Id>& buildOpts,
			springai::Resource* resM, springai::Resource* resE);
	virtual ~CCircuitDef();

	void Init(CCircuitAI* circuit);

	Id GetId() const { return id; }
	springai::UnitDef* GetDef() const { return def; }

	void SetMainRole(RoleT type) { mainRole = type; }
	RoleT GetMainRole() const { return mainRole; }
	void AddEnemyRole(RoleT type) { enemyRole |= GetMask(type); }
	void AddEnemyRoles(RoleM mask) { enemyRole |= mask; }
	bool IsEnemyRoleAny(RoleM value) const { return (enemyRole & value) != 0; }

	void AddAttribute(AttrType type) { attr |= GetMask(static_cast<AttrT>(type)); }
	void AddRole(RoleT type) { AddRole(type, type); }
	void AddRole(RoleT type, RoleT bindType);
	bool IsRespRoleAny(RoleM value)     const { return (respRole & value) != 0; }
//	bool IsRoleAny(RoleM value)     const { return (role & value) != 0; }
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

	bool IsHoldFire()   const { return fireState == FireType::HOLD; }
	bool IsReturnFire() const { return fireState == FireType::RETURN; }
	bool IsOpenFire()   const { return fireState == FireType::OPEN; }

	const std::unordered_set<Id>& GetBuildOptions() const { return buildOptions; }
	float GetBuildDistance() const { return buildDistance; }
	float GetBuildSpeed() const { return buildSpeed; }
	inline bool CanBuild(Id buildDefId) const {	return buildOptions.find(buildDefId) != buildOptions.end(); }
	inline bool CanBuild(CCircuitDef* buildDef) const { return CanBuild(buildDef->GetId()); }
	int GetCount() const { return count; }

	void Inc() { ++count; }
	void Dec() { --count; }

	CCircuitDef& operator++();     // prefix  (++C): no parameter, returns a reference
//	CCircuitDef  operator++(int);  // postfix (C++): dummy parameter, returns a value
	CCircuitDef& operator--();     // prefix  (--C): no parameter, returns a reference
//	CCircuitDef  operator--(int);  // postfix (C--): dummy parameter, returns a value
	bool operator==(const CCircuitDef& rhs) { return id == rhs.id; }
	bool operator!=(const CCircuitDef& rhs) { return id != rhs.id; }

	void SetMaxThisUnit(int value) { maxThisUnit = value; }
	int GetMaxThisUnit() const { return maxThisUnit; }
	void SetSinceFrame(int frame) { sinceFrame = frame; }
	bool IsAvailable() const { return maxThisUnit > count; }
	bool IsAvailable(int frame) const { return IsAvailable() && (frame >= sinceFrame); }

	void IncBuild() { ++buildCounts; }
	void DecBuild() { --buildCounts; }
	int GetBuildCount() const { return buildCounts; }

	bool HasDGun() const { return hasDGun; }
	bool HasDGunAA() const { return hasDGunAA; }
	CWeaponDef* GetDGunDef() const { return dgunDef; }
	springai::WeaponMount* GetDGunMount() const { return dgunMount; }
	springai::WeaponMount* GetShieldMount() const { return shieldMount; }
	springai::WeaponMount* GetWeaponMount() const { return weaponMount; }
	float GetPwrDamage() const { return pwrDmg; }  // ally
	float GetThrDamage() const { return thrDmg; }  // enemy
	float GetAoe() const { return aoe; }
	float GetPower() const { return power; }
	float GetThreat() const { return threat; }
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
	int GetNoChaseCategory() const { return noChaseCategory; }

	void ModPower(float mod) { pwrDmg *= mod; power *= mod; }
	void ModThreat(float mod) { thrDmg *= mod; threat *= mod; }
	void SetThreatRange(ThreatType type, int range) { threatRange[static_cast<ThreatT>(type)] = range; }
	void SetFireState(FireType ft) { fireState = ft; }
	void SetReloadTime(int time) { reloadTime = time; }

	STerrainMapImmobileType::Id GetImmobileId() const { return immobileTypeId; }
	STerrainMapMobileType::Id GetMobileId() const { return mobileTypeId; }

	void SetIgnore(bool value) { isIgnore = value; }
	bool IsIgnore() const { return isIgnore; }

	bool IsAttacker()   const { return isAttacker; }
	bool HasAntiAir()   const { return hasAntiAir; }
	bool HasAntiLand()  const { return hasAntiLand; }
	bool HasAntiWater() const { return hasAntiWater; }
	bool IsAlwaysHit()  const { return isAlwaysHit; }

	bool IsMobile()        const { return speed > .1f; }
	bool IsAbleToFly()     const { return isAbleToFly; }
	bool IsPlane()         const { return isPlane; }
	bool IsFloater()       const { return isFloater; }
	bool IsSubmarine()     const { return isSubmarine; }
	bool IsAmphibious()    const { return isAmphibious; }
	bool IsLander()        const { return isLander; }
	bool IsSonarStealth()  const { return isSonarStealth; }
	bool IsTurnLarge()     const { return isTurnLarge; }
	bool IsAbleToCloak()   const { return isAbleToCloak; }
	bool IsAbleToJump()    const { return isAbleToJump; }
	bool IsAbleToRepair()  const { return isAbleToRepair; }
	bool IsAbleToReclaim() const { return isAbleToReclaim; }
	bool IsAbleToAssist()  const { return isAbleToAssist; }
	bool IsAssistable()    const { return buildTime < 1e6f; }

	void SetIsMex(bool value) { isMex = value; }
	bool IsMex() const { return isMex;}
	void SetIsPylon(bool value) { isPylon = value; }
	bool IsPylon() const { return isPylon;}

	float GetHealth()    const { return health; }
	float GetSpeed()     const { return speed; }
	float GetLosRadius() const { return losRadius; }
	float GetCostM()     const { return costM; }
	float GetCostE()     const { return costE; }
	float GetCloakCost() const { return cloakCost; }
	float GetStockCost() const { return stockCost; }
	float GetBuildTime() const { return buildTime; }
//	float GetAltitude()  const { return altitude; }
	float GetJumpRange() const { return jumpRange; }

	void SetRetreat(float value) { retreat = value; }
	float GetRetreat()   const { return retreat; }

	float GetRadius();
	float GetHeight();
	bool IsYTargetable(float elevation, float posY);
	const springai::AIFloat3& GetMidPosOffset() const { return midPosOffset; }

private:
	static RoleName* roleNames;
	static AttrName attrNames;
	static FireName fireNames;

	// TODO: associate script data with CCircuitDef
	//       instead of using map<Id,data> everywhere
//	friend class CInitScript;
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
	int count;
	int buildCounts;  // number of builder defs able to build this def;
	int maxThisUnit;
	int sinceFrame;

	CWeaponDef* dgunDef;
	springai::WeaponMount* dgunMount;
	springai::WeaponMount* shieldMount;
	springai::WeaponMount* weaponMount;
	float pwrDmg;  // ally damage
	float thrDmg;  // enemy damage
	float aoe;  // radius
	float power;  // ally max threat
	float threat;  // enemy max threat
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
	int noChaseCategory;

	STerrainMapImmobileType::Id immobileTypeId;
	STerrainMapMobileType::Id   mobileTypeId;

	// ---- Bit fields ---- BEGIN
	bool isIgnore : 1;

	bool isAttacker : 1;
	bool hasDGun : 1;
	bool hasDGunAA : 1;

	bool hasAntiAir : 1;  // air layer
	bool hasAntiLand : 1;  // surface (water and land)
	bool hasAntiWater : 1;  // under water
	bool isAlwaysHit : 1;  // FIXME: calc per weapon

	bool isPlane : 1;  // no hover attack
	bool isFloater : 1;
	bool isSubmarine : 1;
	bool isAmphibious : 1;
	bool isLander : 1;
	bool isSonarStealth : 1;
	bool isTurnLarge : 1;
	bool isAbleToFly : 1;
	bool isAbleToCloak : 1;
	bool isAbleToJump : 1;
	bool isAbleToRepair : 1;
	bool isAbleToReclaim : 1;
	bool isAbleToAssist : 1;

	bool isMex : 1;
	bool isPylon : 1;
	// ---- Bit fields ---- END

	float health;
	float speed;
	float losRadius;
	float costM;
	float costE;
	float cloakCost;
	float stockCost;
	float buildTime;
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
