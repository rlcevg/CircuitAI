/*
 * RetreatTask.h
 *
 *  Created on: Jan 18, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_RETREATTASK_H_
#define SRC_CIRCUIT_TASK_RETREATTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CRetreatTask: public IUnitTask {
public:
	CRetreatTask(ITaskManager* mgr);
	virtual ~CRetreatTask();

	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
	virtual void Close(bool done);

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);

private:
	std::set<CCircuitUnit*> updateUnits;
	unsigned int updateSlice;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_RETREATTASK_H_
