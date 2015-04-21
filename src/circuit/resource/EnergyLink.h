/*
 * EnergyLink.h
 *
 *  Created on: Apr 19, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCE_ENERGYLINK_H_
#define SRC_CIRCUIT_RESOURCE_ENERGYLINK_H_

#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"

#include <set>
#include <unordered_map>

namespace circuit {

class CCircuitAI;

class CEnergyLink {
public:
	using Structure = struct {
		CCircuitUnit::Id unitId;
		CCircuitDef::Id cdefId;
		springai::AIFloat3 pos;
	};
	struct cmp {
	   bool operator()(const Structure& lhs, const Structure& rhs) {
	      return lhs.unitId < rhs.unitId;
	   }
	};

	CEnergyLink(CCircuitAI* circuit);
	virtual ~CEnergyLink();

	void Update();

private:
	CCircuitAI* circuit;

	int markFrame;
	std::set<Structure, cmp> markedAllies;
	std::unordered_map<CCircuitDef::Id, float> pylonDefIds;

	void MarkAllyPylons();
	void MarkPylon(const Structure& building, bool alive);
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCE_ENERGYLINK_H_
