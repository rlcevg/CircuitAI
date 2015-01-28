/*
 * AttackTask.h
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_ATTACKTASK_H_
#define SRC_CIRCUIT_TASK_ATTACKTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CAttackTask: public IUnitTask {
public:
	CAttackTask();
	virtual ~CAttackTask();

	virtual void Update(CCircuitAI* circuit);

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_ATTACKTASK_H_
