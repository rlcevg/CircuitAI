/*
 * PatrolTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_PATROLTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_PATROLTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBPatrolTask: public IBuilderTask {
public:
	CBPatrolTask(ITaskManager* mgr, Priority priority,
				 const springai::AIFloat3& position,
				 float cost, int timeout);
	virtual ~CBPatrolTask();

	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;  // FIXME: Remove when proper task assignment implemented

	virtual void Execute(CCircuitUnit* unit) override;
	virtual void Update() override;
protected:
	virtual void Finish() override;
	virtual void Cancel() override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_PATROLTASK_H_
