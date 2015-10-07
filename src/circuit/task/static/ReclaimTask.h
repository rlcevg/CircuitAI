/*
 * ReclaimTask.h
 *
 *  Created on: Mar 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_STATIC_RECLAIMTASK_H_
#define SRC_CIRCUIT_TASK_STATIC_RECLAIMTASK_H_

#include "task/builder/ReclaimTask.h"

namespace circuit {

class CSReclaimTask: public CBReclaimTask {
public:
	CSReclaimTask(ITaskManager* mgr, Priority priority,
				  const springai::AIFloat3& position,
				  float cost, int timeout, float radius = .0f);
	virtual ~CSReclaimTask();

	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);

private:
	unsigned int updCount;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_STATIC_RECLAIMTASK_H_
