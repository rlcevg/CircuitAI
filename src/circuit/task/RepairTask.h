/*
 * RepairTask.h
 *
 *  Created on: Jan 29, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_REPAIRTASK_H_
#define SRC_CIRCUIT_TASK_REPAIRTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CRepairTask: public IUnitTask {
public:
	CRepairTask(Priority priority, float cost, CCircuitUnit* target);
	virtual ~CRepairTask();

	virtual void Update(CCircuitAI* circuit);

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

private:
	float buildPower;
	float cost;
	springai::AIFloat3 position;
	CCircuitUnit* target;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_REPAIRTASK_H_
