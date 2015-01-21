/*
 * RetreatTask.h
 *
 *  Created on: Jan 18, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_RETREATTASK_H_
#define SRC_CIRCUIT_TASK_RETREATTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CRetreatTask: public IUnitTask {
public:
	CRetreatTask();
	virtual ~CRetreatTask();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_RETREATTASK_H_
