/*
 * CircuitUnit.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef CIRCUITUNIT_H_
#define CIRCUITUNIT_H_

namespace springai {
	class Unit;
}

namespace circuit {

class CUnitTask;

class CCircuitUnit {
public:
	CCircuitUnit(springai::Unit* unit);
	virtual ~CCircuitUnit();

	springai::Unit* GetUnit();

private:
	springai::Unit* unit;
	CUnitTask* task;
};

} // namespace circuit

#endif // CIRCUITUNIT_H_
