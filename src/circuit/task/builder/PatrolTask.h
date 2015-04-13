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

	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
	virtual void Close(bool done);
protected:
	virtual void Finish();
	virtual void Cancel();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_PATROLTASK_H_
