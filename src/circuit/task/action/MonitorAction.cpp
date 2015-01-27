/*
 * MonitorAction.cpp
 *
 *  Created on: Jan 27, 2015
 *      Author: rlcevg
 */

#include "task/action/MonitorAction.h"
#include "task/UnitTask.h"
#include "util/utils.h"

namespace circuit {

CMonitorAction::CMonitorAction(IUnitTask* owner) :
		ITaskAction(owner)
{
}

CMonitorAction::~CMonitorAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CMonitorAction::Update(CCircuitAI* circuit)
{
}

} // namespace circuit
