/*
 * TaskAction.cpp
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#include "task/action/TaskAction.h"
#include "task/UnitTask.h"

namespace circuit {

ITaskAction::ITaskAction(IUnitTask* owner) :
		IAction(owner)
{
}

ITaskAction::~ITaskAction()
{
}

} // namespace circuit
