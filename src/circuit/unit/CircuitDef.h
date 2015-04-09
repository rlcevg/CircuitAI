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

	Id GetId() const;
	springai::UnitDef* GetUnitDef() const;
	const std::unordered_set<Id>& GetBuildOptions() const;
	bool CanBuild(Id buildDefId) const;
	bool CanBuild(CCircuitDef* buildDef) const;
	int GetCount() const;

	void Inc();
	void Dec();

	CCircuitDef& operator++();     // prefix  (++C): no parameter, returns a reference
	CCircuitDef  operator++(int);  // postfix (C++): dummy parameter, returns a value
	CCircuitDef& operator--();     // prefix  (++C): no parameter, returns a reference
	CCircuitDef  operator--(int);  // postfix (C++): dummy parameter, returns a value
	inline bool operator==(const CCircuitDef& rhs);
	inline bool operator!=(const CCircuitDef& rhs);

	bool IsAvailable() const;
	void IncBuild();
	void DecBuild();
	int GetBuildCount() const;

	void SetImmobileId(STerrainMapImmobileType::Id immobileId);
	STerrainMapImmobileType::Id GetImmobileId() const;
	void SetMobileId(STerrainMapMobileType::Id mobileId);
	STerrainMapMobileType::Id GetMobileId() const;

private:
	Id id;
	springai::UnitDef* def;  // owner
	std::unordered_set<Id> buildOptions;
	int count;
	int buildCounts;  // number of builder defs able to build this def;

	STerrainMapImmobileType::Id immobileTypeId;
	STerrainMapMobileType::Id   mobileTypeId;
};

inline bool CCircuitDef::operator==(const CCircuitDef& rhs)
{
	return (id == rhs.id);
}

inline bool CCircuitDef::operator!=(const CCircuitDef& rhs)
{
	return (id != rhs.id);
}

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
