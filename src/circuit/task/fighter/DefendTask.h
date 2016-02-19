/*
 * DefendTask.h
 *
 *  Created on: Feb 12, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_DEFENDTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_DEFENDTASK_H_

#include "task/fighter/SquadTask.h"

namespace circuit {

class CDefendTask: public ISquadTask {
public:
	CDefendTask(ITaskManager* mgr, const springai::AIFloat3& position, float maxPower);
	virtual ~CDefendTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
protected:
	virtual void Cancel();

private:
	float maxPower;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_DEFENDTASK_H_
