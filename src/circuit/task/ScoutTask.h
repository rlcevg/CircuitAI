/*
 * ScoutTask.h
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_SCOUTTASK_H_
#define SRC_CIRCUIT_TASK_SCOUTTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CScoutTask: public IUnitTask {
public:
	CScoutTask();
	virtual ~CScoutTask();

	virtual void Update(CCircuitAI* circuit);

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_SCOUTTASK_H_
