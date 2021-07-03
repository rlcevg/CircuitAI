/*
 * WaitTask.h
 *
 *  Created on: May 29, 2017
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_STATIC_WAITTASK_H_
#define SRC_CIRCUIT_TASK_STATIC_WAITTASK_H_

#include "task/common/WaitTask.h"

namespace circuit {

class CSWaitTask: public IWaitTask {
public:
	CSWaitTask(ITaskManager* mgr, bool stop, int timeout);
	virtual ~CSWaitTask();

	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_STATIC_WAITTASK_H_
