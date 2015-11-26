/*
 * TerraformTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_TERRAFORMTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_TERRAFORMTASK_H_

#include "task/builder/BuilderTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

class CBTerraformTask: public IBuilderTask {
public:
	CBTerraformTask(ITaskManager* mgr, Priority priority, CCircuitUnit* target, float cost = 1.0f, int timeout = 0);
	CBTerraformTask(ITaskManager* mgr, Priority priority, const springai::AIFloat3& position, float cost = 1.0f, int timeout = 0);
	virtual ~CBTerraformTask();

	virtual void Execute(CCircuitUnit* unit);
protected:
	virtual void Cancel();

public:
	virtual void OnUnitIdle(CCircuitUnit* unit);

protected:
	CCircuitUnit::Id targetId;  // Ignore "target" as it could be destroyed
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_TERRAFORMTASK_H_
