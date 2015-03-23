/*
 * RepairTask.h
 *
 *  Created on: Jan 29, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_REPAIRTASK_H_
#define SRC_CIRCUIT_TASK_REPAIRTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CEconomyManager;

class CBRepairTask: public IBuilderTask {
public:
	CBRepairTask(ITaskManager* mgr, Priority priority, int timeout = 0);
	virtual ~CBRepairTask();

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
	virtual void Cancel();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);

	virtual void SetTarget(CCircuitUnit* unit);

private:
	CCircuitUnit* FindUnitToAssist(CCircuitUnit* unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_REPAIRTASK_H_
