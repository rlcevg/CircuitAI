/*
 * GenericTask.h
 *
 *  Created on: Jan 19, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_GENERICTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_GENERICTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBGenericTask: public IBuilderTask {
public:
	CBGenericTask(ITaskManager* mgr, BuildType buildType, Priority priority,
				  CCircuitDef* buildDef, const springai::AIFloat3& position,
				  float cost, float shake, int timeout);
	virtual ~CBGenericTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_GENERICTASK_H_
