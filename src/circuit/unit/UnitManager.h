/*
 * UnitManager.h
 *
 *  Created on: Jan 15, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_UNITMANAGER_H_
#define SRC_CIRCUIT_UNIT_UNITMANAGER_H_

namespace circuit {

class CCircuitAI;

class IUnitManager {
protected:
	IUnitManager();
public:
	virtual ~IUnitManager();

	virtual CCircuitAI* GetCircuit() = 0;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_UNITMANAGER_H_
