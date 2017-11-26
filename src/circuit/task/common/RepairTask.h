/*
 * RepairTask.h
 *
 *  Created on: Sep 4, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_COMMON_REPAIRTASK_H_
#define SRC_CIRCUIT_TASK_COMMON_REPAIRTASK_H_

#include "task/builder/BuilderTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

class IRepairTask: public IBuilderTask {
public:
	IRepairTask(ITaskManager* mgr, Priority priority, Type type, CAllyUnit* target, int timeout = 0);
	virtual ~IRepairTask();

	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update() = 0;
protected:
	virtual void Finish() override;
	virtual void Cancel() override final;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) = 0;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker) = 0;

	virtual void SetTarget(CAllyUnit* unit);
	ICoreUnit::Id GetTargetId() const { return targetId; }

protected:
	CAllyUnit* FindUnitToAssist(CCircuitUnit* unit);
	ICoreUnit::Id targetId;  // Ignore "target" variable because ally units are vivid
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_COMMON_REPAIRTASK_H_
