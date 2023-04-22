/*
 * SonarTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_SONARTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_SONARTASK_H_

#include "task/common/SensorTask.h"

namespace circuit {

class CBSonarTask: public ISensorTask {
public:
	CBSonarTask(ITaskManager* mgr, Priority priority,
				CCircuitDef* buildDef, const springai::AIFloat3& position,
				SResource cost, float shake, int timeout);
	CBSonarTask(ITaskManager* mgr);  // Load
	virtual ~CBSonarTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_SONARTASK_H_
