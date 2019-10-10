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

CBBunkerTask::CBBunkerTask(ITaskManager* mgr, Priority priority,
						   CCircuitDef* buildDef, const AIFloat3& position,
						   float cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::BUNKER, cost, shake, timeout)
{
}

CBBunkerTask::~CBBunkerTask()
{
}

} // namespace circuit
