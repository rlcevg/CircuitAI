/*
 * NilTask.h
 *
 *  Created on: May 29, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_NILTASK_H_
#define SRC_CIRCUIT_TASK_NILTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CNilTask: public IUnitTask {
public:
	CNilTask(ITaskManager* mgr);
	virtual ~CNilTask();

	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;
	virtual void Stop(bool done) override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker) override;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_NILTASK_H_
