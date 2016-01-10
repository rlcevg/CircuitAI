/*
 * SonarTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_SONARTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_SONARTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBSonarTask: public IBuilderTask {
public:
	CBSonarTask(ITaskManager* mgr, Priority priority,
				CCircuitDef* buildDef, const springai::AIFloat3& position,
				float cost, bool isShake, int timeout);
	virtual ~CBSonarTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_SONARTASK_H_
