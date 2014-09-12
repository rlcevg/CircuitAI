/*
 * FactoryTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "FactoryTask.h"
#include "utils.h"

namespace circuit {

using namespace springai;

CFactoryTask::CFactoryTask(Priority priority, int difficulty, AIFloat3& position, float radius, float metal, TaskType type, std::list<IConstructTask*>* owner) :
		IConstructTask(priority, difficulty, position, radius, metal, owner),
		type(type)
{
}

CFactoryTask::~CFactoryTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

CFactoryTask::TaskType CFactoryTask::GetType()
{
	return type;
}

} // namespace circuit
