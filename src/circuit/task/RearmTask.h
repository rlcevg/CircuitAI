/*
 * RearmTask.h
 *
 *  Created on: Jan 26, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_REARMTASK_H_
#define SRC_CIRCUIT_TASK_REARMTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CRearmTask: public IUnitTask {
public:
	CRearmTask(ITaskManager* mgr);
	virtual ~CRearmTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_REARMTASK_H_
