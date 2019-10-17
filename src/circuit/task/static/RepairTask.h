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
	CSRepairTask(ITaskManager* mgr, Priority priority, CAllyUnit* target, int timeout = 0);
	virtual ~CSRepairTask();

	virtual void AssignTo(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;
protected:
	virtual void Finish() override final;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_STATIC_REPAIRTASK_H_
