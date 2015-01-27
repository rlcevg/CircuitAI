/*
 * AssignAction.h
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_ACTION_ASSIGNACTION_H_
#define SRC_CIRCUIT_TASK_ACTION_ASSIGNACTION_H_

#include "task/action/TaskAction.h"

namespace circuit {

class CAssignAction: public ITaskAction {
public:
	CAssignAction(IUnitTask* owner);
	virtual ~CAssignAction();

	virtual void Update(CCircuitAI* circuit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_ACTION_ASSIGNACTION_H_
