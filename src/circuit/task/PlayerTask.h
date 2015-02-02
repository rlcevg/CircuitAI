/*
 * PlayerTask.h
 *
 *  Created on: Feb 3, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_PLAYERTASK_H_
#define SRC_CIRCUIT_TASK_PLAYERTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CPlayerTask: public IUnitTask {
public:
	CPlayerTask(ITaskManager* mgr);
	virtual ~CPlayerTask();

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_PLAYERTASK_H_
