/*
 * StaticRepair.h
 *
 *  Created on: Mar 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_STATICREPAIR_H_
#define SRC_CIRCUIT_TASK_BUILDER_STATICREPAIR_H_

#include "task/builder/RepairTask.h"

namespace circuit {

class CStaticRepair: public CBRepairTask {
public:
	CStaticRepair(ITaskManager* mgr, Priority priority, int timeout = 0);
	virtual ~CStaticRepair();

	virtual void OnUnitIdle(CCircuitUnit* unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_STATICREPAIR_H_
