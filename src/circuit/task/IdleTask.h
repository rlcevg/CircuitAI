/*
 * IdleTask.h
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_IDLETASK_H_
#define SRC_CIRCUIT_TASK_IDLETASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CIdleTask: public IUnitTask {
public:
	CIdleTask(CCircuitAI* circuit);
	virtual ~CIdleTask();

	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);
	virtual void MarkCompleted();

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_IDLETASK_H_
