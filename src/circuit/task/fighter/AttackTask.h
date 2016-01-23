/*
 * AttackTask.h
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_ATTACKTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_ATTACKTASK_H_

#include "task/fighter/SquadTask.h"

namespace circuit {

struct STerrainMapArea;

class CAttackTask: public ISquadTask {
public:
	CAttackTask(ITaskManager* mgr);
	virtual ~CAttackTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit) {}
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);

private:
	void FindTarget(CCircuitUnit* unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_ATTACKTASK_H_
