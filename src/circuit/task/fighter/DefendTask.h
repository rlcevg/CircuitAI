/*
 * DefendTask.h
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_DEFENDTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_DEFENDTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CDefendTask: public IFighterTask {
public:
	CDefendTask(ITaskManager* mgr);
	virtual ~CDefendTask();

	virtual void Execute(CCircuitUnit* unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_DEFENDTASK_H_
