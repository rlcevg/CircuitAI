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

	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
	virtual void Close(bool done);

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_NILTASK_H_
