/*
 * RadarTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_RADARTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_RADARTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBRadarTask: public IBuilderTask {
public:
	CBRadarTask(ITaskManager* mgr, Priority priority,
				CCircuitDef* buildDef, const springai::AIFloat3& position,
				float cost, float shake, int timeout);
	virtual ~CBRadarTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_RADARTASK_H_
