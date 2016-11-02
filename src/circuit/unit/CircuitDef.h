/*
 * CircuitDef.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
#define SRC_CIRCUIT_UNIT_CIRCUITDEF_H_

#include "terrain/TerrainData.h"

#include "UnitDef.h"

#include <unordered_set>
#include <array>

namespace springai {
	class WeaponMount;
}

namespace circuit {

class CCircuitDef {
public:
	using Id = int;
	enum class RangeType: char {MAX = 0, AIR = 1, LAND = 2, WATER = 3, _SIZE_};
	using RangeT = std::underlying_type<RangeType>::type;

	// TODO: Rebuild response system on unit vs unit basis (opposed to role vs role).
	// Not implemented: mine, transport
	// No special task: air, sub, static, heavy
	enum class RoleType: unsigned int {BUILDER = 0, SCOUT, RAIDER, RIOT,
									   ASSAULT, SKIRM, ARTY, AA,
									   AS, AH, BOMBER, SUPPORT,
									   MINE, TRANS, AIR, SUB,
									   STATIC, HEAVY, _SIZE_};
	enum RoleMask: unsigned int {BUILDER = 0x00000001, SCOUT = 0x00000002, RAIDER = 0x00000004, RIOT    = 0x00000008,
								 ASSAULT = 0x00000010, SKIRM = 0x00000020, ARTY   = 0x00000040, AA      = 0x00000080,
								 AS      = 0x00000100, AH    = 0x00000200, BOMBER = 0x00000400, SUPPORT = 0x00000800,
								 MINE    = 0x00001000, TRANS = 0x00002000, AIR    = 0x00004000, SUB     = 0x00008000,
								 STATIC  = 0x00010000, HEAVY = 0x00020000, NONE   = 0x00000000};
	using RoleT = std::underlying_type<RoleType>::type;
	using RoleM = std::underlying_type<RoleMask>::type;

	/*
	 * MELEE:      always move close to target, disregard attack range
	 * SIEGE:      mostly use Fight instead of Move
	 * OPEN_FIRE:  always fire at enemy
	 * NO_JUMP:    disable jump on retreat
	 * BOOST:      boost speed on retreat
	 * COMM:       commander
	 * HOLD_FIRE:  hold fire on retreat
	 * NO_STRAFE:  disable gunship's strafe
	 * STOCK:      stockpile weapon before any task (Not implemented)
	 * SUPER:      superweapon
	 */
	enum class AttrType: RoleT {MELEE = static_cast<RoleT>(RoleType::_SIZE_), SIEGE,
								NO_JUMP, BOOST, COMM, NO_STRAFE,
								STOCK, SUPER, RETR_HOLD, _SIZE_};
	enum AttrMask: RoleM {MELEE   = 0x00040000, SIEGE = 0x00080000,
						  NO_JUMP = 0x00100000, BOOST = 0x00200000, COMM      = 0x00400000, NO_STRAFE = 0x00800000,
						  STOCK   = 0x01000000, SUPER = 0x02000000, RETR_HOLD = 0x04000000};

	enum FireType: int {HOLD = 0, RETURN = 1, OPEN = 2, _SIZE_};

	static RoleM GetMask(RoleT type) { return 1 << type; }

	using RoleName = std::map<std::string, RoleType>;
	using AttrName = std::map<std::string, AttrType>;
	using FireName = std::map<std::string, FireType>;
	static RoleName& GetRoleNames() { return roleNames; }
	static AttrName& GetAttrNames() { return attrNames; }
	static FireName& GetFireNames() { return fireNames; }

	CCircuitDef(const CCircuitDef& that) = delete;
	CCircuitDef& operator=(const CCircuitDef&) = delete;
	CCircuitDef(CCircuitAI* circuit, springai::UnitDef* def, std::unordered_set<Id>& buildOpts, springai::Resource* res);
	virtual ~CCircuitDef();

	void Init(CCircuitAI* circuit);

	Id GetId() const { return id; }
	springai::UnitDef* GetUnitDef() const { return def; }

	void SetMainRole(RoleType type) { mainRole = type; }
	RoleT GetMainRole() const { return static_cast<RoleT>(mainRole); }
	void SetEnemyRole(RoleType type) { enemyRole = type; }
	RoleT GetEnemyRole() const { return static_cast<RoleT>(enemyRole); }

	void AddAttribute(AttrType value) { role |= GetMask(static_cast<RoleT>(value)); }
	void AddRole(RoleType value) { role |= GetMask(static_cast<RoleT>(value)); }
	bool IsRoleAny(RoleM value)     const { return (role & value) != 0; }
	bool IsRoleEqual(RoleM value)   const { return role == value; }
	bool IsRoleContain(RoleM value) const { return (role & value) == value; }

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

	bool IsAttrMelee()    const { return role & AttrMask::MELEE; }
	bool IsAttrSiege()    const { return role & AttrMask::SIEGE; }
	bool IsAttrNoJump()   const { return role & AttrMask::NO_JUMP; }
	bool IsAttrBoost()    const { return role & AttrMask::BOOST; }
	bool IsAttrComm()     const { return role & AttrMask::COMM; }
	bool IsAttrNoStrafe() const { return role & AttrMask::NO_STRAFE; }
	bool IsAttrStock()    const { return role & AttrMask::STOCK; }
	bool IsAttrSuper()    const { return role & AttrMask::SUPER; }
	bool IsAttrRetrHold() const { return role & AttrMask::RETR_HOLD; }

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
	bool IsAvailable() const { return maxThisUnit > count; }

	void IncBuild() { ++buildCounts; }
	void DecBuild() { --buildCounts; }
	int GetBuildCount() const { return buildCounts; }

	bool HasDGun() const { return hasDGun; }
	bool HasDGunAA() const { return hasDGunAA; }
//	int GetDGunReload() const { return dgunReload; }
	float GetDGunRange() const { return dgunRange; }
	springai::WeaponMount* GetDGunMount() const { return dgunMount; }
	springai::WeaponMount* GetShieldMount() const { return shieldMount; }
	springai::WeaponMount* GetWeaponMount() const { return weaponMount; }
	float GetDPS() const { return dps; }
	float GetDamage() const { return dmg; }
	float GetAoe() const { return aoe; }
	float GetPower() const { return power; }
	float GetMinRange() const { return minRange; }
	float GetMaxRange(RangeType type = RangeType::MAX) const { return maxRange[static_cast<RangeT>(type)]; }
	float GetShieldRadius() const { return shieldRadius; }
	float GetMaxShield() const { return maxShield; }
	int GetFireState() const { return fireState; }
	int GetReloadTime() const { return reloadTime; }
	int GetCategory() const { return category; }
	int GetTargetCategory() const { return targetCategory; }
	int GetNoChaseCategory() const { return noChaseCategory; }

	void ModDamage(float mod) { dmg *= mod; power *= mod; }
	void SetFireState(FireType ft) { fireState = ft; }
	void SetReloadTime(int time) { reloadTime = time; }

	STerrainMapImmobileType::Id GetImmobileId() const { return immobileTypeId; }
	STerrainMapMobileType::Id GetMobileId() const { return mobileTypeId; }

	bool IsAttacker()   const { return dps > .1f; }
	bool HasAntiAir()   const { return hasAntiAir; }
	bool HasAntiLand()  const { return hasAntiLand; }
	bool HasAntiWater() const { return hasAntiWater; }

	bool IsMobile()       const { return speed > .1f; }
	bool IsAbleToFly()    const { return isAbleToFly; }
	bool IsPlane()        const { return isPlane; }
	bool IsFloater()      const { return isFloater; }
	bool IsSubmarine()    const { return isSubmarine; }
	bool IsAmphibious()   const { return isAmphibious; }
	bool IsLander()       const { return isLander; }
	bool IsSonarStealth() const { return isSonarStealth; }
	bool IsTurnLarge()    const { return isTurnLarge; }
	bool IsAbleToCloak()  const { return isAbleToCloak; }
	bool IsAbleToJump()   const { return isAbleToJump; }
	bool IsAssistable()   const { return buildTime < 1e6f; }

	float GetSpeed()     const { return speed; }
	float GetLosRadius() const { return losRadius; }
	float GetCost()      const { return cost; }
	float GetCloakCost() const { return cloakCost; }
	float GetStockCost() const { return stockCost; }
	float GetBuildTime() const { return buildTime; }
//	float GetAltitude()  const { return altitude; }
	float GetJumpRange() const { return jumpRange; }

	void SetRetreat(float value) { retreat = value; }
	float GetRetreat()   const { return retreat; }

	const springai::AIFloat3& GetMidPosOffset() const { return midPosOffset; }

private:
	static RoleName roleNames;
	static AttrName attrNames;
	static FireName fireNames;

	Id id;
	springai::UnitDef* def;  // owner
	RoleType mainRole;
	RoleType enemyRole;
	RoleM role;
	std::unordered_set<Id> buildOptions;
	float buildDistance;
	float buildSpeed;
	int count;
	int buildCounts;  // number of builder defs able to build this def;
	int maxThisUnit;

	bool hasDGun;
	bool hasDGunAA;
//	int dgunReload;  // frames in ticks
	float dgunRange;
	springai::WeaponMount* dgunMount;
	springai::WeaponMount* shieldMount;
	springai::WeaponMount* weaponMount;
	float dps;  // TODO: split dps like ranges on air, land, water
	float dmg;
	float aoe;  // radius
	float power;  // attack power = UnitDef's max threat
	float minRange;
	std::array<float, static_cast<RangeT>(RangeType::_SIZE_)> maxRange;
	float shieldRadius;
	float maxShield;
	FireType fireState;
	int reloadTime;  // frames in ticks
	int category;
	int targetCategory;
	int noChaseCategory;

	STerrainMapImmobileType::Id immobileTypeId;
	STerrainMapMobileType::Id   mobileTypeId;

	bool hasAntiAir;  // air layer
	bool hasAntiLand;  // surface (water and land)
	bool hasAntiWater;  // under water

	// TODO: Use bit field?
	bool isPlane;  // no hover attack
	bool isFloater;
	bool isSubmarine;
	bool isAmphibious;
	bool isLander;
	bool isSonarStealth;
	bool isTurnLarge;
	bool isAbleToFly;
	bool isAbleToCloak;
	bool isAbleToJump;

	float speed;
	float losRadius;
	float cost;
	float cloakCost;
	float stockCost;
	float buildTime;
//	float altitude;
	float jumpRange;
	float retreat;

	springai::AIFloat3 midPosOffset;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
