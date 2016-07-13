/*
 * GuardTask.h
 *
 *  Created on: Jul 13, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_GUARDTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_GUARDTASK_H_

#include "task/builder/BuilderTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

class CBGuardTask: public IBuilderTask {
public:
	CBGuardTask(ITaskManager* mgr, Priority priority,
				CCircuitUnit* vip, int timeout);
	virtual ~CBGuardTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);

	virtual void OnUnitIdle(CCircuitUnit* unit);

private:
	CCircuitUnit::Id vipId;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_GUARDTASK_H_
