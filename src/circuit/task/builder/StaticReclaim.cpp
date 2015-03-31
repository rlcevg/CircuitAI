/*
 * StaticReclaim.cpp
 *
 *  Created on: Mar 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/StaticReclaim.h"
#include "task/TaskManager.h"
#include "util/utils.h"

namespace circuit {

CStaticReclaim::CStaticReclaim(ITaskManager* mgr, Priority priority,
							   springai::UnitDef* buildDef, const springai::AIFloat3& position,
							   float cost, int timeout, float radius) :
		CBReclaimTask(mgr, priority, buildDef, position, cost, timeout, radius)
{
}

CStaticReclaim::~CStaticReclaim()
{
//	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CStaticReclaim::OnUnitIdle(CCircuitUnit* unit)
{
	manager->DoneTask(this);
}

} // namespace circuit
