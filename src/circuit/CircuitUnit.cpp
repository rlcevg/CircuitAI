/*
 * CircuitUnit.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "CircuitUnit.h"
#include "UnitTask.h"
#include "utils.h"

#include "Unit.h"
#include "UnitDef.h"

namespace circuit {

using namespace springai;

CCircuitUnit::CCircuitUnit(Unit* unit, UnitDef* def) :
		unit(unit),
		def(def),
		task(nullptr)
{
}

CCircuitUnit::~CCircuitUnit()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete unit;
}

Unit* CCircuitUnit::GetUnit()
{
	return unit;
}

UnitDef* CCircuitUnit::GetDef()
{
	return def;
}

} // namespace circuit
