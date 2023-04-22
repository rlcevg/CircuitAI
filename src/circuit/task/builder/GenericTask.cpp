/*
 * GenericTask.cpp
 *
 *  Created on: Jan 19, 2021
 *      Author: rlcevg
 */

#include "task/builder/GenericTask.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

CBGenericTask::CBGenericTask(ITaskManager* mgr, BuildType buildType, Priority priority,
							 CCircuitDef* buildDef, const AIFloat3& position,
							 SResource cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, buildType, cost, shake, timeout)
{
}

CBGenericTask::CBGenericTask(ITaskManager* mgr, BuildType buildType)
		: IBuilderTask(mgr, Type::BUILDER, buildType)
{
}

CBGenericTask::~CBGenericTask()
{
}

} // namespace circuit
