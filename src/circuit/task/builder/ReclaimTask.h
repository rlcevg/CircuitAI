/*
 * ReclaimTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_RECLAIMTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_RECLAIMTASK_H_

#include "task/common/ReclaimTask.h"

namespace circuit {

class CBReclaimTask: public IReclaimTask {
public:
	CBReclaimTask(ITaskManager* mgr, Priority priority,
				  const springai::AIFloat3& position,
				  float cost, int timeout, float radius = .0f, bool isMetal = true);
	CBReclaimTask(ITaskManager* mgr, Priority priority,
				  CCircuitUnit* target,
				  int timeout);
	virtual ~CBReclaimTask();

	virtual void Update() override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_RECLAIMTASK_H_
