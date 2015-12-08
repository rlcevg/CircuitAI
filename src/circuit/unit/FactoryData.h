/*
 * FactoryData.h
 *
 *  Created on: Dec 8, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_FACTORYDATA_H_
#define SRC_CIRCUIT_UNIT_FACTORYDATA_H_

#include "unit/CircuitDef.h"

namespace circuit {

class CCircuitAI;

class CFactoryData {
public:
	CFactoryData(CCircuitAI *circuit);
	virtual ~CFactoryData();

	CCircuitDef* GetFactoryToBuild(CCircuitAI* circuit);
	void AdvanceFactoryIdx() { ++factoryIdx %= factoryBuilds.size(); }

private:
	struct SFactory {
		CCircuitDef::Id id;
		float startImp;  // importance[0]
		float switchImp;  // importance[1]
	};
	std::vector<SFactory> factoryBuilds;
	int factoryIdx;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_FACTORYDATA_H_
