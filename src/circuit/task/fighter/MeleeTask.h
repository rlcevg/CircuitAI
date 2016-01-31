/*
 * MeleeTask.h
 *
 *  Created on: Jan 29, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_MELEETASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_MELEETASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CMeleeTask: public IFighterTask {
public:
	CMeleeTask(ITaskManager* mgr);
	virtual ~CMeleeTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);

private:
	void Execute(CCircuitUnit* unit, bool isUpdating);
	CEnemyUnit* FindTarget(CCircuitUnit* unit, const springai::AIFloat3& pos, F3Vec& path);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_MELEETASK_H_
