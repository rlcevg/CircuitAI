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

	enum class RoleType: unsigned int {BUILDER = 0, SCOUT, RAIDER, RIOT, ASSAULT, SKIRM, ARTY, AA, AH, MINE, SUPPORT, TRANS, AIR, STATIC, HEAVY, _SIZE_};
	enum RoleMask: unsigned int {BUILDER = 0x0001, SCOUT   = 0x0002, RAIDER  = 0x0004, RIOT   = 0x0008,
								 ASSAULT = 0x0010, SKIRM   = 0x0020, ARTY    = 0x0040, AA     = 0x0080,
								 AH      = 0x0100, MINE    = 0x0200, SUPPORT = 0x0400, TRANS  = 0x0800,
								 AIR     = 0x1000, STATIC  = 0x2000, HEAVY   = 0x4000, NONE   = 0x0000};
	using RoleT = std::underlying_type<RoleType>::type;
	using RoleM = std::underlying_type<RoleMask>::type;

	/*
	 * BOMBER:     special bomber task assignment and target selection
	 * MELEE:      always move close to target, disregard attack range
	 * SIEGE:      use Fight on retreat instead of Move
	 * HOLD_FIRE:  hold fire on retreat
	 * STOCKPILE:  load weapon before any task (NOT IMPLEMENTED)
	 * "limit<N>": limits number of units by <N> (applied on init)
	 */
	enum class AttrType: RoleT {BOMBER = static_cast<RoleT>(RoleType::_SIZE_), MELEE, SIEGE, HOLD_FIRE, STOCK, _SIZE_};
	enum AttrMask: RoleM {BOMBER = 0x08000,
						  MELEE  = 0x10000, SIEGE = 0x20000, HOLD_FIRE = 0x40000, STOCK = 0x80000};

	static RoleM GetMask(RoleT type) { return 1 << type; }

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
	bool IsRoleArty()     const { return role & RoleMask::ARTY; }
	bool IsRoleAA()       const { return role & RoleMask::AA; }
	bool IsRoleAH()       const { return role & RoleMask::AH; }

	bool IsAttrBomber()   const { return role & AttrMask::BOMBER; }
	bool IsAttrMelee()    const { return role & AttrMask::MELEE; }
	bool IsAttrSiege()    const { return role & AttrMask::SIEGE; }
	bool IsAttrHoldFire() const { return role & AttrMask::HOLD_FIRE; }

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
	float GetPower() const { return power; }
	float GetMaxRange(RangeType type = RangeType::MAX) const { return maxRange[static_cast<RangeT>(type)]; }
	float GetMaxShield() const { return maxShield; }
	int GetCategory() const { return category; }
	int GetTargetCategory() const { return targetCategory; }
	int GetNoChaseCategory() const { return noChaseCategory; }

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

	float GetSpeed()     const { return speed; }
	float GetLosRadius() const { return losRadius; }
	float GetCost()      const { return cost; }

	void SetRetreat(float value) { retreat = value; }
	float GetRetreat()   const { return retreat; }

	const springai::AIFloat3& GetMidPosOffset() const { return midPosOffset; }

private:
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
	float power;  // attack power = UnitDef's max threat
	std::array<float, static_cast<RangeT>(RangeType::_SIZE_)> maxRange;
	float maxShield;
	int category;
	int targetCategory;
	int noChaseCategory;

	STerrainMapImmobileType::Id immobileTypeId;
	STerrainMapMobileType::Id   mobileTypeId;

	bool hasAntiAir;  // air layer
	bool hasAntiLand;  // surface (water and land)
	bool hasAntiWater;  // under water

	// TODO: Use bit field?
	bool isAbleToFly;
	bool isPlane;  // no hover attack
	bool isFloater;
	bool isSubmarine;
	bool isAmphibious;
	bool isLander;
	bool isSonarStealth;
	bool isTurnLarge;

	float speed;
	float losRadius;
	float cost;
	float retreat;

	springai::AIFloat3 midPosOffset;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
