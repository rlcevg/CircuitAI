/*
 * RepairTask.h
 *
 *  Created on: Jan 29, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_REPAIRTASK_H_
#define SRC_CIRCUIT_TASK_REPAIRTASK_H_

#include "task/common/RepairTask.h"

namespace circuit {

class CEconomyManager;

class CBRepairTask: public IRepairTask {
public:
	CBRepairTask(ITaskManager* mgr, Priority priority, CAllyUnit* target, int timeout = 0);
	virtual ~CBRepairTask();

	virtual void Start(CCircuitUnit* unit) override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;

private:
	virtual bool Reevaluate(CCircuitUnit* unit) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_REPAIRTASK_H_
