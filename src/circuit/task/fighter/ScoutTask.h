/*
 * ScoutTask.h
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_SCOUTTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_SCOUTTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CScoutTask: public IUnitTask {
public:
	CScoutTask(ITaskManager* mgr);
	virtual ~CScoutTask();

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_SCOUTTASK_H_
