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
	CBBunkerTask(ITaskManager* mgr, Priority priority,
				 CCircuitDef* buildDef, const springai::AIFloat3& position,
				 float cost, float shake, int timeout);
	CBBunkerTask(ITaskManager* mgr);  // Load
	virtual ~CBBunkerTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_BUNKERTASK_H_
