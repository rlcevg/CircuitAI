/*
 * FactoryTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/FactoryTask.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CBFactoryTask::CBFactoryTask(CCircuitAI* circuit, Priority priority,
						   UnitDef* buildDef, const AIFloat3& position,
						   BuildType type, float cost, int timeout) :
		IBuilderTask(circuit, priority, buildDef, position, BuildType::FACTORY, cost, timeout)
{
}

CBFactoryTask::~CBFactoryTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

} // namespace circuit
