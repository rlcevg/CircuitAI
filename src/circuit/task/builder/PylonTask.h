/*
 * PylonTask.h
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_PYLONTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_PYLONTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBPylonTask: public IBuilderTask {
public:
	CBPylonTask(CCircuitAI* circuit, Priority priority,
				springai::UnitDef* buildDef, const springai::AIFloat3& position,
				float cost, int timeout);
	virtual ~CBPylonTask();

	virtual void Execute(CCircuitUnit* unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_PYLONTASK_H_
