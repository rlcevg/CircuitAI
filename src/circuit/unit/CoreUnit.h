/*
 * Unit.h
 *
 *  Created on: Nov 25, 2017
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_COREUNIT_H_
#define SRC_CIRCUIT_UNIT_COREUNIT_H_

#include "Unit.h"

namespace circuit {

class CCircuitDef;

class ICoreUnit {
public:
	using Id = int;

	ICoreUnit(Id unitId, springai::Unit* unit, CCircuitDef* cdef)
		: id(unitId)
		, unit(unit)
		, circuitDef(cdef)
	{}
	virtual ~ICoreUnit() { delete unit; }

	Id GetId() const { return id; }
	springai::Unit* GetUnit() const { return unit; }
	CCircuitDef* GetCircuitDef() const { return circuitDef; }

	bool operator==(const ICoreUnit& rhs) { return id == rhs.GetId(); }
	bool operator!=(const ICoreUnit& rhs) { return id != rhs.GetId(); }

protected:
	Id id;
	springai::Unit* unit;  // owner
	CCircuitDef* circuitDef;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_COREUNIT_H_
