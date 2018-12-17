/*
 * SensorTask.h
 *
 *  Created on: Nov 9, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_COMMON_SENSORTASK_H_
#define SRC_CIRCUIT_TASK_COMMON_SENSORTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class ISensorTask: public IBuilderTask {
public:
	ISensorTask(ITaskManager* mgr, Priority priority,
				CCircuitDef* buildDef, const springai::AIFloat3& position, BuildType buildType,
				float cost, float shake, int timeout);
	virtual ~ISensorTask();

	virtual void Update() override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_COMMON_SENSORTASK_H_
