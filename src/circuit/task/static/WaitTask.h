/*
 * WaitTask.h
 *
 *  Created on: Jul 24, 2016
 *      Author: evgenij
 */

#ifndef SRC_CIRCUIT_TASK_STATIC_WAITTASK_H_
#define SRC_CIRCUIT_TASK_STATIC_WAITTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CWaitTask: public IUnitTask {
public:
	CWaitTask(ITaskManager* mgr, int timeout);
	virtual ~CWaitTask();

	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_STATIC_WAITTASK_H_
