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
	CIdleTask();
	virtual ~CIdleTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);

public:
	static CIdleTask* IdleTask;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_IDLETASK_H_
