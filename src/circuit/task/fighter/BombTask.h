/*
 * BombTask.h
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_BOMBTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_BOMBTASK_H_

#include "task/fighter/SquadTask.h"

namespace circuit {

class CBombTask: public ISquadTask {
public:
	CBombTask(IUnitModule* mgr, float powerMod);
	virtual ~CBombTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;

private:
	void FindTarget();
	void ApplyTargetPath(const CQueryPathSingle* query);
	void FallbackBasePos();
	void ApplyBasePos(const CQueryPathSingle* query);
	void Fallback();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_BOMBTASK_H_
