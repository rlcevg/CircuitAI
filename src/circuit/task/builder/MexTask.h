/*
 * MexTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_MEXTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_MEXTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBMexTask: public IBuilderTask {
public:
	CBMexTask(ITaskManager* mgr, Priority priority,
			  springai::UnitDef* buildDef, const springai::AIFloat3& position,
			  float cost, int timeout);
	virtual ~CBMexTask();

	// TODO: Prevent from building enemy's mex
	virtual void Execute(CCircuitUnit* unit);
	virtual void Finish();
	virtual void Cancel();

	virtual void OnUnitIdle(CCircuitUnit* unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_MEXTASK_H_
