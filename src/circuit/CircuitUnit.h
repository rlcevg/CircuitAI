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
	class UnitDef;
}

namespace circuit {

class CUnitTask;

class CCircuitUnit {
public:
	CCircuitUnit(springai::Unit* unit, springai::UnitDef* def);
	virtual ~CCircuitUnit();

	springai::Unit* GetUnit();
	springai::UnitDef* GetDef();

private:
	springai::Unit* unit;
	springai::UnitDef* def;
	CUnitTask* task;
};

} // namespace circuit

#endif // CIRCUITUNIT_H_
