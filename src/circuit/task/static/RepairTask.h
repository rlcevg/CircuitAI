/*
 * RepairTask.h
 *
 *  Created on: Mar 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_STATIC_REPAIRTASK_H_
#define SRC_CIRCUIT_TASK_STATIC_REPAIRTASK_H_

#include "task/builder/RepairTask.h"

namespace circuit {

class CSRepairTask: public CBRepairTask {
public:
	CSRepairTask(ITaskManager* mgr, Priority priority, CCircuitUnit* target, int timeout = 0);
	virtual ~CSRepairTask();

	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);

private:
	int updCount;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_STATIC_REPAIRTASK_H_
