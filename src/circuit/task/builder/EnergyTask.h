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
	CBEnergyTask(CCircuitAI* circuit, Priority priority,
				 springai::UnitDef* buildDef, const springai::AIFloat3& position,
				 BuildType type, float cost, int timeout);
	virtual ~CBEnergyTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_ENERGYTASK_H_
