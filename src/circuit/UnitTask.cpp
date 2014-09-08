/*
 * UnitTask.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "UnitTask.h"
#include "CircuitUnit.h"
#include "utils.h"

namespace circuit {

CUnitTask::CUnitTask() :
		unit(nullptr)
{
}

CUnitTask::~CUnitTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

} // namespace circuit
