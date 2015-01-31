/*
 * BunkerTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_BUNKERTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_BUNKERTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBBunkerTask: public IBuilderTask {
public:
	CBBunkerTask(CCircuitAI* circuit, Priority priority,
				 springai::UnitDef* buildDef, const springai::AIFloat3& position,
				 BuildType type, float cost, int timeout);
	virtual ~CBBunkerTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_BUNKERTASK_H_
