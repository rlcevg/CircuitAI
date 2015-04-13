/*
 * ReclaimTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_RECLAIMTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_RECLAIMTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBReclaimTask: public IBuilderTask {
public:
	CBReclaimTask(ITaskManager* mgr, Priority priority,
				  const springai::AIFloat3& position,
				  float cost, int timeout, float radius = .0f);
	virtual ~CBReclaimTask();

	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
	virtual void Close(bool done);
protected:
	virtual void Finish();
	virtual void Cancel();

protected:
	float radius;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_RECLAIMTASK_H_
