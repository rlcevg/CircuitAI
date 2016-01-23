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
	enum class RangeType: char {MAX = 0, AIR = 1, LAND = 2, WATER = 3, COUNT};
	enum RoleType: int {BUILDER = 0x0001, SCOUT   = 0x0002, RAIDER  = 0x0004, RIOT = 0x0008,
						ASSAULT = 0x0010, HEAVY   = 0x0020, SKIRM   = 0x0040, ARTY = 0x0080,
						AA      = 0x0100, BOMBER  = 0x0200, STATIC  = 0x0400, NONE = 0x0000};

	CCircuitDef(const CCircuitDef& that) = delete;
	CCircuitDef& operator=(const CCircuitDef&) = delete;
	CCircuitDef(CCircuitAI* circuit, springai::UnitDef* def, std::unordered_set<Id>& buildOpts, springai::Resource* res);
	virtual ~CCircuitDef();

	void Init(CCircuitAI* circuit);

	Id GetId() const { return id; }
	springai::UnitDef* GetUnitDef() const { return def; }

	void SetRole(RoleType value) { role |= value; }
	std::underlying_type<RoleType>::type GetRole() const { return role; }
	bool IsRoleBuilder() const { return role & RoleType::BUILDER; }
	bool IsRoleScout()   const { return role & RoleType::SCOUT; }
	bool IsRoleRaider()  const { return role & RoleType::RAIDER; }
	bool IsRoleRiot()    const { return role & RoleType::RIOT; }
	bool IsRoleAssault() const { return role & RoleType::ASSAULT; }
	bool IsRoleArty()    const { return role & RoleType::ARTY; }
	bool IsRoleAA()      const { return role & RoleType::AA; }
	bool IsRoleBomber()  const { return role & RoleType::BOMBER; }

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

	bool IsManualFire() const { return isManualFire; }
	bool HasDGunAA() const { return hasDGunAA; }
//	int GetDGunReload() const { return dgunReload; }
	float GetDGunRange() const { return dgunRange; }
	springai::WeaponMount* GetDGunMount() const { return dgunMount; }
	springai::WeaponMount* GetShieldMount() const { return shieldMount; }
	springai::WeaponMount* GetWeaponMount() const { return weaponMount; }
	float GetDPS() const { return dps; }
	float GetPower() const { return power; }
	float GetMaxRange(RangeType type = RangeType::MAX) const { return maxRange[static_cast<unsigned>(type)]; }
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
	std::underlying_type<RoleType>::type role;
	std::unordered_set<Id> buildOptions;
	float buildDistance;
	float buildSpeed;
	int count;
	int buildCounts;  // number of builder defs able to build this def;
	int maxThisUnit;

	bool isManualFire;
	bool hasDGunAA;
//	int dgunReload;  // frames in ticks
	float dgunRange;
	springai::WeaponMount* dgunMount;
	springai::WeaponMount* shieldMount;
	springai::WeaponMount* weaponMount;
	float dps;  // TODO: split dps like ranges on air, land, water
	float power;  // attack power = UnitDef's max threat
	std::array<float, static_cast<unsigned>(RangeType::COUNT)> maxRange;
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
