/*
 * DefendTask.h
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_DEFENDTASK_H_
#define SRC_CIRCUIT_TASK_DEFENDTASK_H_

#include "task/UnitTask.h"

namespace circuit
{

class CDefendTask: public IUnitTask
{
public:
	CDefendTask();
	virtual ~CDefendTask();

	virtual void Update(CCircuitAI* circuit);

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_DEFENDTASK_H_
