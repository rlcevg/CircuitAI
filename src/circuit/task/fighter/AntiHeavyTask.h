/*
 * AntiHeavyTask.h
 *
 *  Created on: Jun 30, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_ANTIHEAVYTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_ANTIHEAVYTASK_H_

#include "task/fighter/SquadTask.h"

namespace circuit {

class CAntiHeavyTask: public ISquadTask {
public:
	CAntiHeavyTask(ITaskManager* mgr, float powerMod);
	virtual ~CAntiHeavyTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;

private:
	bool FindTarget();
	void ApplyTargetPath(const CQueryPathMulti* query);
	void FallbackTargetEmpty();
	void FallbackAttackSafe();
	void ApplyAttackSafe(const CQueryPathMulti* query);
	void FallbackStaticSafe();
	void ApplyStaticSafe(const CQueryPathMulti* query);
	void FallbackBasePos();
	void ApplyBasePos(const CQueryPathSingle* query);
	void FallbackCommPos();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_ANTIHEAVYTASK_H_
