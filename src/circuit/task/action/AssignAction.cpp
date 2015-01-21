/*
 * AssignAction.cpp
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#include "task/action/AssignAction.h"
#include "task/UnitTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "util/utils.h"

namespace circuit {

CAssignAction::CAssignAction(IUnitTask* owner) :
		ITaskAction(owner)
{
}

CAssignAction::~CAssignAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CAssignAction::Update()
{
	IUnitTask* task = static_cast<IUnitTask*>(ownerList);
	for (auto ass : task->GetAssignees()) {
		IUnitManager* manager = ass->GetManager();
		manager->AssignTask(ass);
		manager->ExecuteTask(ass);
	}
	task->MarkCompleted();

//	isFinished = true;
}

} // namespace circuit
