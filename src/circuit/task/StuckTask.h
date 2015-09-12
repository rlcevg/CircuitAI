/*
 * StuckTask.h
 *
 *  Created on: Sep 12, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_STUCKTASK_H_
#define SRC_CIRCUIT_TASK_STUCKTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CStuckTask: public IUnitTask {
public:
	CStuckTask(ITaskManager* mgr);
	virtual ~CStuckTask();

	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_STUCKTASK_H_
