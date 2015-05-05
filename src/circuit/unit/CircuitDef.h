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

	inline Id GetId() const { return id; }
	inline springai::UnitDef* GetUnitDef() const { return def; }
	inline const std::unordered_set<Id>& GetBuildOptions() const { return buildOptions; }
	inline float GetBuildDistance() const { return buildDistance; }
	inline bool CanBuild(Id buildDefId) const;
	inline bool CanBuild(CCircuitDef* buildDef) const;
	inline int GetCount() const { return count; }

	inline void Inc() { ++count; }
	inline void Dec() { --count; }

	CCircuitDef& operator++();     // prefix  (++C): no parameter, returns a reference
	CCircuitDef  operator++(int);  // postfix (C++): dummy parameter, returns a value
	CCircuitDef& operator--();     // prefix  (++C): no parameter, returns a reference
	CCircuitDef  operator--(int);  // postfix (C++): dummy parameter, returns a value
	inline bool operator==(const CCircuitDef& rhs);
	inline bool operator!=(const CCircuitDef& rhs);

	inline bool IsAvailable() const;
	inline void IncBuild() { ++buildCounts; }
	inline void DecBuild() { --buildCounts; }
	inline int GetBuildCount() const { return buildCounts; }

	inline void SetImmobileId(STerrainMapImmobileType::Id immobileId) { immobileTypeId = immobileId; }
	inline STerrainMapImmobileType::Id GetImmobileId() const { return immobileTypeId; }
	inline void SetMobileId(STerrainMapMobileType::Id mobileId) { mobileTypeId = mobileId; }
	inline STerrainMapMobileType::Id GetMobileId() const { return mobileTypeId; }

private:
	Id id;
	springai::UnitDef* def;  // owner
	std::unordered_set<Id> buildOptions;
	float buildDistance;
	int count;
	int buildCounts;  // number of builder defs able to build this def;

	STerrainMapImmobileType::Id immobileTypeId;
	STerrainMapMobileType::Id   mobileTypeId;
};

inline bool CCircuitDef::CanBuild(Id buildDefId) const
{
	return (buildOptions.find(buildDefId) != buildOptions.end());
}

inline bool CCircuitDef::CanBuild(CCircuitDef* buildDef) const
{
	// FIXME: Remove Patrol/Reclaim/Terra tasks from CBuildManager::builderTasks
	return (buildDef != nullptr) ? CanBuild(buildDef->GetId()) : false;
}

inline bool CCircuitDef::operator==(const CCircuitDef& rhs)
{
	return (id == rhs.id);
}

inline bool CCircuitDef::operator!=(const CCircuitDef& rhs)
{
	return (id != rhs.id);
}

inline bool CCircuitDef::IsAvailable() const
{
	return (def->GetMaxThisUnit() > count);
}

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
