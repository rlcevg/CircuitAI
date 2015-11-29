/*
 * EnergyTask.h
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_ENERGYTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_ENERGYTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBEnergyTask: public IBuilderTask {
public:
	CBEnergyTask(ITaskManager* mgr, Priority priority,
				 CCircuitDef* buildDef, const springai::AIFloat3& position,
				 float cost, bool isShake, int timeout);
	virtual ~CBEnergyTask();

	virtual void Update();

protected:
	virtual void Finish();

	bool isStalling;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_ENERGYTASK_H_
