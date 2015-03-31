/*
 * StaticRepair.cpp
 *
 *  Created on: Mar 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/StaticRepair.h"
#include "task/TaskManager.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CStaticRepair::CStaticRepair(ITaskManager* mgr, Priority priority, int timeout) :
		CBRepairTask(mgr, priority, timeout)
{
}

CStaticRepair::~CStaticRepair()
{
//	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CStaticRepair::OnUnitIdle(CCircuitUnit* unit)
{
	manager->DoneTask(this);
}

} // namespace circuit
