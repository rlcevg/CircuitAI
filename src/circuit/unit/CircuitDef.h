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

namespace springai {
	class WeaponMount;
}

namespace circuit {

class CCircuitDef {
public:
	using Id = int;

	CCircuitDef(const CCircuitDef& that) = delete;
	CCircuitDef& operator=(const CCircuitDef&) = delete;
	CCircuitDef(CCircuitAI* circuit, springai::UnitDef* def, std::unordered_set<Id>& buildOpts, springai::Resource* res);
	virtual ~CCircuitDef();

	Id GetId() const { return id; }
	springai::UnitDef* GetUnitDef() const { return def; }
	const std::unordered_set<Id>& GetBuildOptions() const { return buildOptions; }
	float GetBuildDistance() const { return buildDistance; }
	inline bool CanBuild(Id buildDefId) const;
	inline bool CanBuild(CCircuitDef* buildDef) const;
	int GetCount() const { return count; }

	void Inc() { ++count; }
	void Dec() { --count; }

	CCircuitDef& operator++();     // prefix  (++C): no parameter, returns a reference
//	CCircuitDef  operator++(int);  // postfix (C++): dummy parameter, returns a value
	CCircuitDef& operator--();     // prefix  (--C): no parameter, returns a reference
//	CCircuitDef  operator--(int);  // postfix (C--): dummy parameter, returns a value
	bool operator==(const CCircuitDef& rhs) { return id == rhs.id; }
	bool operator!=(const CCircuitDef& rhs) { return id != rhs.id; }

	bool IsAvailable() const { return def->GetMaxThisUnit() > count; }
	void IncBuild() { ++buildCounts; }
	void DecBuild() { --buildCounts; }
	int GetBuildCount() const { return buildCounts; }

//	int GetDGunReload() const { return dgunReload; }
	float GetDGunRange() const { return dgunRange; }
	springai::WeaponMount* GetDGunMount() const { return dgunMount; }
	float GetDPS() const { return dps; }

	void SetImmobileId(STerrainMapImmobileType::Id immobileId) { immobileTypeId = immobileId; }
	STerrainMapImmobileType::Id GetImmobileId() const { return immobileTypeId; }
	void SetMobileId(STerrainMapMobileType::Id mobileId) { mobileTypeId = mobileId; }
	STerrainMapMobileType::Id GetMobileId() const { return mobileTypeId; }

	bool IsAntiAir()   const { return isAntiAir; }
	bool IsAntiLand()  const { return isAntiLand; }
	bool IsAntiWater() const { return isAntiWater; }

	bool IsMobile() const { return isMobile; }

	float GetCost() const { return cost; }

private:
	Id id;
	springai::UnitDef* def;  // owner
	std::unordered_set<Id> buildOptions;
	float buildDistance;
	int count;
	int buildCounts;  // number of builder defs able to build this def;

//	int dgunReload;  // frames in ticks
	float dgunRange;
	springai::WeaponMount* dgunMount;
	float dps;

	STerrainMapImmobileType::Id immobileTypeId;
	STerrainMapMobileType::Id   mobileTypeId;

	bool isAntiAir;
	bool isAntiLand;
	bool isAntiWater;

	bool isMobile;

	float cost;
};

inline bool CCircuitDef::CanBuild(Id buildDefId) const
{
	return buildOptions.find(buildDefId) != buildOptions.end();
}

inline bool CCircuitDef::CanBuild(CCircuitDef* buildDef) const
{
	// FIXME: Remove Patrol/Reclaim/Terra tasks from CBuildManager::builderTasks
	return (buildDef != nullptr) ? CanBuild(buildDef->GetId()) : false;
}

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
