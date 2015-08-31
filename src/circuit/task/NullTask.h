/*
 * NullTask.h
 *
 *  Created on: May 29, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_NULLTASK_H_
#define SRC_CIRCUIT_TASK_NULLTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CNullTask: public IUnitTask {
public:
	CNullTask(ITaskManager* mgr);
	virtual ~CNullTask();

	virtual void AssignTo(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_NULLTASK_H_
