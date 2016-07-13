/*
 * SupportTask.h
 *
 *  Created on: Jul 3, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_SUPPORTTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_SUPPORTTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CSupportTask: public IFighterTask {
public:
	CSupportTask(ITaskManager* mgr);
	virtual ~CSupportTask();

	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

private:
	unsigned int updCount;
	bool isWait;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_SUPPORTTASK_H_
