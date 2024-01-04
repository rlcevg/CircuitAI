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

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;

	virtual void OnRearmStart(CCircuitUnit* unit) override;

private:
	void Execute(CCircuitUnit* unit, bool isUpdating);
	springai::AIFloat3 FindTarget(CCircuitUnit* unit, CEnemyInfo* lastTarget, const springai::AIFloat3& pos);
	void ApplyTargetPath(const CQueryPathSingle* query, bool isUpdating);
	void FallbackScout(CCircuitUnit* unit, bool isUpdating);
	void ApplyScoutPath(const CQueryPathSingle* query);
	void Fallback(CCircuitUnit* unit, bool proceed);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_BOMBTASK_H_
