/*
 * DefenceTask.h
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_DEFENCETASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_DEFENCETASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBDefenceTask: public IBuilderTask {
public:
	CBDefenceTask(CCircuitAI* circuit, Priority priority,
				  springai::UnitDef* buildDef, const springai::AIFloat3& position,
				  BuildType type, float cost, int timeout);
	virtual ~CBDefenceTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_DEFENCETASK_H_
