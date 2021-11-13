/*
 * StoreTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/StoreTask.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

CBStoreTask::CBStoreTask(ITaskManager* mgr, Priority priority,
						 CCircuitDef* buildDef, const AIFloat3& position,
						 float cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::STORE, cost, shake, timeout)
{
}

CBStoreTask::CBStoreTask(ITaskManager* mgr)
		: IBuilderTask(mgr, Type::BUILDER, BuildType::STORE)
{
}

CBStoreTask::~CBStoreTask()
{
}

} // namespace circuit
