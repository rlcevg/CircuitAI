/*
 * MilitaryTask.h
 *
 *  Created on: Apr 10, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_MILITARYTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_MILITARYTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBMilitaryTask: public IBuilderTask {
public:
	CBMilitaryTask(ITaskManager* mgr);
	virtual ~CBMilitaryTask();

	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_MILITARYTASK_H_
