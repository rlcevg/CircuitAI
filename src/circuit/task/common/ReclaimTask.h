/*
 * ReclaimTask.h
 *
 *  Created on: Sep 4, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_COMMON_RECLAIMTASK_H_
#define SRC_CIRCUIT_TASK_COMMON_RECLAIMTASK_H_

#include "task/builder/BuilderTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

class IReclaimTask: public IBuilderTask {
public:
	IReclaimTask(ITaskManager* mgr, Priority priority, Type type,
				 const springai::AIFloat3& position,
				 float cost, int timeout, float radius = .0f, bool isMetal = true);
	IReclaimTask(ITaskManager* mgr, Priority priority, Type type,
				 CCircuitUnit* target,
				 int timeout);
	virtual ~IReclaimTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);  // FIXME: Remove when proper task assignment implemented

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update() = 0;
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

#endif // SRC_CIRCUIT_TASK_COMMON_RECLAIMTASK_H_
