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
	CAntiAirTask(IUnitModule* mgr, float powerMod);
	virtual ~CAntiAirTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;

private:
	NSMicroPather::HitFunc GetHitTest() const;
	void FindTarget();
	void FallbackDisengage();
	void ApplyDisengagePath(const CQueryPathSingle* query);
	void ApplyTargetPath(const CQueryPathSingle* query);
	void FallbackSafePos();
	void ApplySafePos(const CQueryPathMulti* query);
	void FallbackCommPos();
	void Fallback();

	void ApplyDamagedPath(const CQueryPathMulti* query);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_ANTIAIRTASK_H_
