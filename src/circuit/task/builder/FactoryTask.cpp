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

CBFactoryTask::CBFactoryTask(ITaskManager* mgr, Priority priority,
							 UnitDef* buildDef, const AIFloat3& position,
							 float cost, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, BuildType::FACTORY, cost, timeout)
{
}

CBFactoryTask::~CBFactoryTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

} // namespace circuit
