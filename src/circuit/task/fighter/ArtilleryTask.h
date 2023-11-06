/*
 * ArtilleryTask.h
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_ARTILLERYTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_ARTILLERYTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CArtilleryTask: public IFighterTask {
public:
	CArtilleryTask(IUnitModule* mgr);
	virtual ~CArtilleryTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;

private:
	void Execute(CCircuitUnit* unit);
	CEnemyInfo* FindTarget(CCircuitUnit* unit, const springai::AIFloat3& pos);
	void ApplyTargetPath(const CQueryPathMulti* query);
	void FallbackSafePos(CCircuitUnit* unit);
	void ApplySafePos(const CQueryPathMulti* query);
	void Fallback(CCircuitUnit* unit, bool proceed);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_ARTILLERYTASK_H_
