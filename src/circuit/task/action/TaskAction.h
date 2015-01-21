/*
 * TaskAction.h
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_ACTION_TASKACTION_H_
#define SRC_CIRCUIT_TASK_ACTION_TASKACTION_H_

#include "util/Action.h"

namespace circuit {

class IUnitTask;

class ITaskAction: public IAction {
protected:
	ITaskAction(IUnitTask* owner);
public:
	virtual ~ITaskAction();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_ACTION_TASKACTION_H_
