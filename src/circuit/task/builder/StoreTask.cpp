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

CBStoreTask::CBStoreTask(IUnitModule* mgr, Priority priority,
						 CCircuitDef* buildDef, const AIFloat3& position,
						 SResource cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::STORE, cost, shake, timeout)
{
}

CBStoreTask::CBStoreTask(IUnitModule* mgr)
		: IBuilderTask(mgr, Type::BUILDER, BuildType::STORE)
{
}

CBStoreTask::~CBStoreTask()
{
}

} // namespace circuit
