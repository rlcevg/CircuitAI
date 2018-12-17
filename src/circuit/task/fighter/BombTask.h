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
	CBombTask(ITaskManager* mgr, float powerMod);
	virtual ~CBombTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Execute(CCircuitUnit* unit) override;
	virtual void Update() override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker) override;

private:
	void Execute(CCircuitUnit* unit, bool isUpdating);
	CEnemyUnit* FindTarget(CCircuitUnit* unit, CEnemyUnit* lastTarget, const springai::AIFloat3& pos, F3Vec& path);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_BOMBTASK_H_
