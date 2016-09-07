/*
 * BombTask.h
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_BOMBTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_BOMBTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CBombTask: public IFighterTask {
public:
	CBombTask(ITaskManager* mgr);
	virtual ~CBombTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);

private:
	void Execute(CCircuitUnit* unit, bool isUpdating);
	CEnemyUnit* FindTarget(CCircuitUnit* unit, CEnemyUnit* lastTarget, const springai::AIFloat3& pos, F3Vec& path);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_BOMBTASK_H_
