/*
 * AssignAction.cpp
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#include "task/action/AssignAction.h"
#include "task/UnitTask.h"
#include "CircuitAI.h"
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

void CAssignAction::Update(CCircuitAI* circuit)
{
	isBlocking = false;
	IUnitTask* task = static_cast<IUnitTask*>(ownerList);
	auto assignees = task->GetAssignees();  // copy assignees
	for (auto ass : assignees) {
		task->RemoveAssignee(ass);
		IUnitManager* manager = ass->GetManager();
		manager->AssignTask(ass);
		manager->ExecuteTask(ass);
		if (!circuit->IsUpdateTimeValid()) {
			isBlocking = true;
			break;
		}
	}
}

} // namespace circuit
