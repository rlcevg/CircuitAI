/*
 * AntiAirTask.h
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_ANTIAIRTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_ANTIAIRTASK_H_

#include "task/fighter/SquadTask.h"

namespace circuit {

class CAntiAirTask: public ISquadTask {
public:
	CAntiAirTask(ITaskManager* mgr);
	virtual ~CAntiAirTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);

private:
	void FindTarget();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_ANTIAIRTASK_H_
