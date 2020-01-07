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

class ICoreUnit {
public:
	using Id = int;

	ICoreUnit(Id unitId, springai::Unit* unit)
		: id(unitId)
		, unit(unit)
	{}

	Id GetId() const { return id; }
	springai::Unit* GetUnit() const { return unit; }

	bool operator==(const ICoreUnit& rhs) { return id == rhs.GetId(); }
	bool operator!=(const ICoreUnit& rhs) { return id != rhs.GetId(); }

protected:
	~ICoreUnit() { delete unit; }

	Id id;
	springai::Unit* unit;  // owner
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_COREUNIT_H_
