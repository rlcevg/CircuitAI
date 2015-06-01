/*
 * CircuitDef.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
#define SRC_CIRCUIT_UNIT_CIRCUITDEF_H_

#include "static/TerrainData.h"

#include "UnitDef.h"

#include <unordered_set>

namespace circuit {

class CCircuitDef {
public:
	using Id = int;

	CCircuitDef(springai::UnitDef* def, std::unordered_set<Id>& buildOpts);
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
	CCircuitDef  operator++(int);  // postfix (C++): dummy parameter, returns a value
	CCircuitDef& operator--();     // prefix  (++C): no parameter, returns a reference
	CCircuitDef  operator--(int);  // postfix (C++): dummy parameter, returns a value
	bool operator==(const CCircuitDef& rhs) { return id == rhs.id; }
	bool operator!=(const CCircuitDef& rhs) { return id != rhs.id; }

	bool IsAvailable() const { return def->GetMaxThisUnit() > count; }
	void IncBuild() { ++buildCounts; }
	void DecBuild() { --buildCounts; }
	int GetBuildCount() const { return buildCounts; }

	int GetReloadFrames() const { return reloadFrames; }
	float GetDGunRange() const { return dgunRange; }

	void SetImmobileId(STerrainMapImmobileType::Id immobileId) { immobileTypeId = immobileId; }
	STerrainMapImmobileType::Id GetImmobileId() const { return immobileTypeId; }
	void SetMobileId(STerrainMapMobileType::Id mobileId) { mobileTypeId = mobileId; }
	STerrainMapMobileType::Id GetMobileId() const { return mobileTypeId; }

private:
	Id id;
	springai::UnitDef* def;  // owner
	std::unordered_set<Id> buildOptions;
	float buildDistance;
	int count;
	int buildCounts;  // number of builder defs able to build this def;

	int reloadFrames;
	float dgunRange;

	STerrainMapImmobileType::Id immobileTypeId;
	STerrainMapMobileType::Id   mobileTypeId;
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
