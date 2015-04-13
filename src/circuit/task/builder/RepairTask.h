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
	CBRepairTask(ITaskManager* mgr, Priority priority, CCircuitUnit* target = nullptr, int timeout = 0);
	virtual ~CBRepairTask();

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
protected:
	virtual void Finish();
	virtual void Cancel();

public:
	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);

	virtual void SetTarget(CCircuitUnit* unit);

protected:
	CCircuitUnit* FindUnitToAssist(CCircuitUnit* unit);
	CCircuitUnit::Id targetId;  // Ignore "target" variable because ally units are vivid
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_REPAIRTASK_H_
