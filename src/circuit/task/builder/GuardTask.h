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
	CBGuardTask(IUnitModule* mgr, Priority priority,
				CCircuitUnit* vip, bool isInterrupt, int timeout);
	virtual ~CBGuardTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Stop(bool done) override;

protected:
	virtual bool Execute(CCircuitUnit* unit) override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;

protected:
	virtual bool Reevaluate(CCircuitUnit* unit);

private:
	bool IsTargetBuilder() const;

	ICoreUnit::Id vipId;
	bool isInterrupt;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_GUARDTASK_H_
