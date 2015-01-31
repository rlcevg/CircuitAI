/*
 * DefenceTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/DefenceTask.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CBDefenceTask::CBDefenceTask(CCircuitAI* circuit, Priority priority,
							 UnitDef* buildDef, const AIFloat3& position,
							 float cost, int timeout) :
		IBuilderTask(circuit, priority, buildDef, position, BuildType::DEFENCE, cost, timeout)
{
}

CBDefenceTask::~CBDefenceTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

} // namespace circuit
