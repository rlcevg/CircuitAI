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
	enum class RangeType: char {MAX = 0, AIR = 1, LAND = 2, WATER = 3, TOTAL_COUNT};
	enum class RoleType: int {BUILDER = 0, RAIDER, RIOT, ASSAULT, SKIRM, ARTY, AIR, STATIC, SCOUT, HEAVY, BOMBER, MELEE, AH, AA, TOTAL_COUNT};
	enum RoleMask: int {BUILDER = 0x0001, RAIDER = 0x0002, RIOT   = 0x0004, ASSAULT = 0x0008,
						SKIRM   = 0x0010, ARTY   = 0x0020, AIR    = 0x0040, STATIC  = 0x0080,
						SCOUT   = 0x0100, HEAVY  = 0x0200, BOMBER = 0x0400, MELEE   = 0x0800,
						AH      = 0x1000, AA     = 0x2000, NONE   = 0x0000};
	using RangeT = std::underlying_type<RangeType>::type;
	using RoleT = std::underlying_type<RoleType>::type;
	using RoleM = std::underlying_type<RoleMask>::type;

	static RoleM GetMask(RoleT type) { return 1 << type; }
	static RoleMask GetMask(RoleType type) { return static_cast<RoleMask>(1 << static_cast<RoleT>(type)); }

	CCircuitDef(const CCircuitDef& that) = delete;
	CCircuitDef& operator=(const CCircuitDef&) = delete;
	CCircuitDef(CCircuitAI* circuit, springai::UnitDef* def, std::unordered_set<Id>& buildOpts, springai::Resource* res);
	virtual ~CCircuitDef();

	void Init(CCircuitAI* circuit);

	Id GetId() const { return id; }
	springai::UnitDef* GetUnitDef() const { return def; }

	void SetMainRole(RoleType type) { mainRole = type; }
	RoleT GetMainRole() const { return static_cast<RoleT>(mainRole); }

	void AddRole(RoleType value) { role |= GetMask(value); }
	bool IsRoleAny(RoleM value)     const { return (role & value) != 0; }
	bool IsRoleEqual(RoleM value)   const { return role == value; }
	bool IsRoleContain(RoleM value) const { return (role & value) == value; }

	bool IsRoleBuilder() const { return role & RoleMask::BUILDER; }
	bool IsRoleScout()   const { return role & RoleMask::SCOUT; }
	bool IsRoleRaider()  const { return role & RoleMask::RAIDER; }
	bool IsRoleRiot()    const { return role & RoleMask::RIOT; }
	bool IsRoleAssault() const { return role & RoleMask::ASSAULT; }
	bool IsRoleBomber()  const { return role & RoleMask::BOMBER; }
	bool IsRoleMelee()   const { return role & RoleMask::MELEE; }
	bool IsRoleArty()    const { return role & RoleMask::ARTY; }
	bool IsRoleAA()      const { return role & RoleMask::AA; }

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

	void SetSiege(bool value)    { isSiege = value; }
	void SetHoldFire(bool value) { isHoldFire = value; }
	bool IsSiege()    const { return isSiege; }
	bool IsHoldFire() const { return isHoldFire; }

	float GetSpeed()     const { return speed; }
	float GetLosRadius() const { return losRadius; }
	float GetCost()      const { return cost; }

	void SetRetreat(float value) { retreat = value; }
	float GetRetreat()   const { return retreat; }

//	const springai::AIFloat3& GetMidPosOffset() const { return midPosOffset; }

private:
	Id id;
	springai::UnitDef* def;  // owner
	RoleType mainRole;
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
	float power;  // attack power = UnitDef's max threat
	std::array<float, static_cast<RangeT>(RangeType::TOTAL_COUNT)> maxRange;
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

	// Retreat options
	bool isSiege;  // Use Fight on retreat instead of Move
	bool isHoldFire;  // Hold fire no retreat

	float speed;
	float losRadius;
	float cost;
	float retreat;

//	springai::AIFloat3 midPosOffset;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
