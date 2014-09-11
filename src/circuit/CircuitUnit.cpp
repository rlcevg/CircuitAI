/*
 * CircuitUnit.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "CircuitUnit.h"
#include "ModuleTask.h"
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

void CCircuitUnit::SetTask(IModuleTask* task)
{
	this->task = task;
}

IModuleTask* CCircuitUnit::GetTask()
{
	return task;
}

} // namespace circuit
