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
	// TODO: Re-evalute params
	CBReclaimTask(ITaskManager* mgr, Priority priority,
				  springai::UnitDef* buildDef, const springai::AIFloat3& position,
				  float cost, int timeout);
	virtual ~CBReclaimTask();

	virtual void RemoveAssignee(CCircuitUnit* unit);
	virtual void Close(bool done);

	virtual void Execute(CCircuitUnit* unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_RECLAIMTASK_H_
