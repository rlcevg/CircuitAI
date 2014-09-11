/*
 * ModuleTask.cpp
 *
 *  Created on: Sep 10, 2014
 *      Author: rlcevg
 */

#include "ModuleTask.h"
#include "UnitTask.h"

namespace circuit {

IModuleTask::IModuleTask()
{
}

IModuleTask::~IModuleTask()
{
	for (auto task : subtasks) {
		delete task;
	}
}

} // namespace circuit
