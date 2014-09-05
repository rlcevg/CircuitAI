/*
 * CircuitUnit.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "CircuitUnit.h"
#include "UnitTask.h"
#include "Unit.h"

namespace circuit {

using namespace springai;

CCircuitUnit::CCircuitUnit(Unit* unit) :
		unit(unit),
		task(nullptr)
{
}

CCircuitUnit::~CCircuitUnit()
{
}

Unit* CCircuitUnit::GetUnit()
{
	return unit;
}

} // namespace circuit
