/*
 * RepairTask.h
 *
 *  Created on: Jan 29, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_REPAIRTASK_H_
#define SRC_CIRCUIT_TASK_REPAIRTASK_H_

#include "task/builder/BuilderTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

class CEconomyManager;

class CBRepairTask: public IBuilderTask {
public:
	CBRepairTask(ITaskManager* mgr, Priority priority, CCircuitUnit* target, int timeout = 0);
	virtual ~CBRepairTask();

	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
protected:
	virtual void Finish() override final;
	virtual void Cancel() override final;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);

	virtual void SetTarget(CCircuitUnit* unit);
	CCircuitUnit::Id GetTargetId() const { return targetId; }

protected:
	CCircuitUnit* FindUnitToAssist(CCircuitUnit* unit);
	CCircuitUnit::Id targetId;  // Ignore "target" variable because ally units are vivid
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_REPAIRTASK_H_
