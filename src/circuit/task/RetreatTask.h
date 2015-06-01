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

	virtual void RemoveAssignee(CCircuitUnit* unit);
	virtual void Close(bool done);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

private:
	std::set<CCircuitUnit*> updateUnits;
	unsigned int updateSlice;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_RETREATTASK_H_
