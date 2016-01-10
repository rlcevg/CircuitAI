/*
 * ArtilleryTask.h
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_ARTILLERYTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_ARTILLERYTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CArtilleryTask: public IFighterTask {
public:
	CArtilleryTask(ITaskManager* mgr);
	virtual ~CArtilleryTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_ARTILLERYTASK_H_
