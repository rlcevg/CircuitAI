/*
 * SensorTask.h
 *
 *  Created on: Nov 9, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_COMMON_SENSORTASK_H_
#define SRC_CIRCUIT_TASK_COMMON_SENSORTASK_H_

#include "task/builder/BuilderTask.h"

#include <functional>

namespace circuit {

class ISensorTask: public IBuilderTask {
public:
	ISensorTask(IUnitModule* mgr, Priority priority, std::function<bool (CCircuitDef*)> isSensor,
				CCircuitDef* buildDef, const springai::AIFloat3& position, BuildType buildType,
				SResource cost, float shake, int timeout);
	ISensorTask(IUnitModule* mgr, std::function<bool (CCircuitDef*)> isSensor, BuildType buildType);  // Load
	virtual ~ISensorTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;

	virtual void Update() override;

protected:
	std::function<bool (CCircuitDef*)> isSensorTest;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_COMMON_SENSORTASK_H_
