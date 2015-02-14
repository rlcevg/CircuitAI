/*
 * CircuitDef.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
#define SRC_CIRCUIT_UNIT_CIRCUITDEF_H_

#include <unordered_set>

namespace springai {
	class UnitDef;
}

namespace circuit {

struct STerrainMapMobileType;
struct STerrainMapImmobileType;

class CCircuitDef {
public:
	CCircuitDef(std::unordered_set<springai::UnitDef*>& opts);
	virtual ~CCircuitDef();

	const std::unordered_set<springai::UnitDef*>& GetBuildOptions() const;
	bool CanBuild(springai::UnitDef* buildDef);
	int GetCount();

	void Inc();
	void Dec();

	CCircuitDef& operator++ ();     // prefix  (++C): no parameter, returns a reference
	CCircuitDef  operator++ (int);  // postfix (C++): dummy parameter, returns a value
	CCircuitDef& operator-- ();     // prefix  (++C): no parameter, returns a reference
	CCircuitDef  operator-- (int);  // postfix (C++): dummy parameter, returns a value

	void IncBuild();
	void DecBuild();
	int GetBuildCount();

	void SetImmobileType(STerrainMapImmobileType* immobile);
	STerrainMapImmobileType* GetImmobileType();
	void SetMobileType(STerrainMapMobileType* mobile);
	STerrainMapMobileType* GetMobileType();

private:
	std::unordered_set<springai::UnitDef*> buildOptions;
	int count;
	int buildCounts;  // number of builder defs able to build this def;

	STerrainMapMobileType* mobileType;
	STerrainMapImmobileType* immobileType;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITDEF_H_
