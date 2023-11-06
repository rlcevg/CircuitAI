/*
 * BunkerTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/BunkerTask.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

CBBunkerTask::CBBunkerTask(IUnitModule* mgr, Priority priority,
						   CCircuitDef* buildDef, const AIFloat3& position,
						   SResource cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::BUNKER, cost, shake, timeout)
{
}

CBBunkerTask::CBBunkerTask(IUnitModule* mgr)
		: IBuilderTask(mgr, Type::BUILDER, BuildType::BUNKER)
{
}

CBBunkerTask::~CBBunkerTask()
{
}

} // namespace circuit
