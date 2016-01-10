/*
 * AntiAirTask.h
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_ANTIAIRTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_ANTIAIRTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CAntiAirTask: public IFighterTask {
public:
	CAntiAirTask(ITaskManager* mgr, float enemyAir);
	virtual ~CAntiAirTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);

private:
	float enemyAir;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_ANTIAIRTASK_H_
