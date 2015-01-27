/*
 * MonitorAction.h
 *
 *  Created on: Jan 27, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_ACTION_MONITORACTION_H_
#define SRC_CIRCUIT_TASK_ACTION_MONITORACTION_H_

#include "task/action/TaskAction.h"

namespace circuit {

class CMonitorAction: public ITaskAction {
public:
	CMonitorAction(IUnitTask* owner);
	virtual ~CMonitorAction();

	virtual void Update(CCircuitAI* circuit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_ACTION_MONITORACTION_H_
