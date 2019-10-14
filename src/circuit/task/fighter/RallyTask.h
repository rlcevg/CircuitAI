/*
 * RallyTask.h
 *
 *  Created on: Jan 18, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_RALLYTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_RALLYTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CRallyTask: public IFighterTask {
public:
	CRallyTask(ITaskManager* mgr, float maxPower);
	virtual ~CRallyTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;

	virtual void Start(CCircuitUnit* unit) override;

private:
	float maxPower;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_RALLYTASK_H_
