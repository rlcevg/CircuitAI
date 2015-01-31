/*
 * TerraformTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_TERRAFORMTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_TERRAFORMTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBTerraformTask: public IBuilderTask {
public:
	// TODO: Re-evalute params
	CBTerraformTask(CCircuitAI* circuit, Priority priority,
					springai::UnitDef* buildDef, const springai::AIFloat3& position,
					BuildType type, float cost, int timeout);
	virtual ~CBTerraformTask();

	virtual void Execute(CCircuitUnit* unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_TERRAFORMTASK_H_
