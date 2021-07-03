/*
 * IdleTask.h
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_IDLETASK_H_
#define SRC_CIRCUIT_TASK_IDLETASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CIdleTask: public IUnitTask {
public:
	CIdleTask(ITaskManager* mgr);
	virtual ~CIdleTask();

	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;
	virtual void Stop(bool done) override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker) override;

private:
	std::set<CCircuitUnit*> updateUnits;
	unsigned int updateSlice;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_IDLETASK_H_
