/*
 * RaidTask.h
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_RAIDTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_RAIDTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CRaidTask: public IFighterTask {
public:
	CRaidTask(ITaskManager* mgr);
	virtual ~CRaidTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_RAIDTASK_H_
