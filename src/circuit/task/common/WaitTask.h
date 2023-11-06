/*
 * WaitTask.h
 *
 *  Created on: Jul 24, 2016
 *      Author: evgenij
 */

#ifndef SRC_CIRCUIT_TASK_WAITTASK_H_
#define SRC_CIRCUIT_TASK_WAITTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class IWaitTask: public IUnitTask {
public:
	IWaitTask(IUnitModule* mgr, bool stop, int timeout);
	virtual ~IWaitTask();

	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker) override;

protected:
	bool isStop;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_WAITTASK_H_
