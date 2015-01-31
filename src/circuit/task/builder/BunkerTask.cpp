/*
 * BunkerTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/BunkerTask.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CBBunkerTask::CBBunkerTask(CCircuitAI* circuit, Priority priority,
						   UnitDef* buildDef, const AIFloat3& position,
						   BuildType type, float cost, int timeout) :
		IBuilderTask(circuit, priority, buildDef, position, BuildType::BUNKER, cost, timeout)
{
}

CBBunkerTask::~CBBunkerTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

} // namespace circuit
