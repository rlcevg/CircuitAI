/*
 * CircuitUnit.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "unit/CircuitUnit.h"
#include "task/UnitTask.h"
#include "util/utils.h"

#include "Unit.h"
#include "UnitDef.h"

namespace circuit {

using namespace springai;

CCircuitUnit::CCircuitUnit(Unit* unit, UnitDef* def, CCircuitDef* circuitDef) :
		unit(unit),
		def(def),
		circuitDef(circuitDef),
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

CCircuitDef* CCircuitUnit::GetCircuitDef()
{
	return circuitDef;
}

void CCircuitUnit::SetTask(IUnitTask* task)
{
	this->task = task;
}

IUnitTask* CCircuitUnit::GetTask()
{
	return task;
}

void CCircuitUnit::RemoveTask()
{
	if (task != nullptr) {
		task->RemoveAssignee(this);
	}
}

} // namespace circuit
