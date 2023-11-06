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
	CBGenericTask(IUnitModule* mgr, BuildType buildType, Priority priority,
				  CCircuitDef* buildDef, const springai::AIFloat3& position,
				  SResource cost, float shake, int timeout);
	CBGenericTask(IUnitModule* mgr, BuildType buildType);  // Load
	virtual ~CBGenericTask();

	virtual bool IsGeneric() const override { return true; }
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_GENERICTASK_H_
