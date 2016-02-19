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
				  float cost, int timeout, float radius = .0f, bool isMetal = true);
	virtual ~CBReclaimTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);  // FIXME: Remove when proper task assignment implemented

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
protected:
	virtual void Finish();
	virtual void Cancel();

public:
	virtual void OnUnitIdle(CCircuitUnit* unit);

protected:
	float radius;
	bool isMetal;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_RECLAIMTASK_H_
