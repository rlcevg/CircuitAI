/*
 * FactoryData.h
 *
 *  Created on: Dec 8, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_FACTORYDATA_H_
#define SRC_CIRCUIT_UNIT_FACTORYDATA_H_

#include "unit/CircuitDef.h"

#include <unordered_map>

namespace circuit {

class CCircuitAI;

class CFactoryData {
public:
	CFactoryData(CCircuitAI *circuit);
	virtual ~CFactoryData();

	CCircuitDef* GetFactoryToBuild(CCircuitAI* circuit, springai::AIFloat3 position = -RgtVector,
								   bool isStart = false, bool isReset = false);
	void AddFactory(const CCircuitDef* cdef);
	void DelFactory(const CCircuitDef* cdef);

private:
	void ReadConfig(CCircuitAI* circuit);

	unsigned int choiceNum;
	unsigned int noAirNum;
	struct SFactory {
		CCircuitDef::Id id;
		float startImp;  // importance[0]
		float switchImp;  // importance[1]
		int count;
		float mapSpeedPerc;
	};
	float airMapPerc;
	float minOffset;
	float lenOffset;
	std::unordered_map<CCircuitDef::Id, SFactory> allFactories;

//	std::unordered_map<CCircuitDef::Id, std::unordered_set<CCircuitDef::Id>> factoryDefs;  // builder: set<factory>
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_FACTORYDATA_H_
