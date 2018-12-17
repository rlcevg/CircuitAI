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

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;  // FIXME: Remove when proper task assignment implemented

	virtual void Execute(CCircuitUnit* unit) override;
	virtual void Update() override = 0;
protected:
	virtual void Finish() override;
	virtual void Cancel() override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;

protected:
	float radius;
	bool isMetal;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_COMMON_RECLAIMTASK_H_
