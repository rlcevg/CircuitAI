/*
 * ScoutTask.h
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_SCOUTTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_SCOUTTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CScoutTask: public IFighterTask {
public:
	CScoutTask(ITaskManager* mgr);
	virtual ~CScoutTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
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

#endif // SRC_CIRCUIT_TASK_FIGHTER_SCOUTTASK_H_
