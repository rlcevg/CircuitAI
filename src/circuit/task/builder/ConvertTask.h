/*
 * ConvertTask.h
 *
 *  Created on: Jun 21, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_CONVERTTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_CONVERTTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBConvertTask: public IBuilderTask {
public:
	CBConvertTask(IUnitModule* mgr, Priority priority,
				  CCircuitDef* buildDef, const springai::AIFloat3& position,
				  SResource cost, float shake, int timeout);
	CBConvertTask(IUnitModule* mgr);  // Load
	virtual ~CBConvertTask();

	virtual void Update() override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_CONVERTTASK_H_
