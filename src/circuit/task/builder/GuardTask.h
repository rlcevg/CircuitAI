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

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

protected:
	virtual void Execute(CCircuitUnit* unit) override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;

private:
	ICoreUnit::Id vipId;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_GUARDTASK_H_
