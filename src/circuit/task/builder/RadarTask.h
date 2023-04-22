/*
 * RadarTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_RADARTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_RADARTASK_H_

#include "task/common/SensorTask.h"

namespace circuit {

class CBRadarTask: public ISensorTask {
public:
	CBRadarTask(ITaskManager* mgr, Priority priority,
				CCircuitDef* buildDef, const springai::AIFloat3& position,
				SResource cost, float shake, int timeout);
	CBRadarTask(ITaskManager* mgr);  // Load
	virtual ~CBRadarTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_RADARTASK_H_
