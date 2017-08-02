/*
 * RepairTask.h
 *
 *  Created on: Mar 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_STATIC_REPAIRTASK_H_
#define SRC_CIRCUIT_TASK_STATIC_REPAIRTASK_H_

#include "task/common/RepairTask.h"

namespace circuit {

class CSRepairTask: public IRepairTask {
public:
	CSRepairTask(ITaskManager* mgr, Priority priority, CCircuitUnit* target, int timeout = 0);
	virtual ~CSRepairTask();

	virtual void Update();
protected:
	virtual void Finish() override final;

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_STATIC_REPAIRTASK_H_
