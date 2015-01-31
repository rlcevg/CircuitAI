/*
 * FactoryTask.h
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FACTORYTASK_H_
#define SRC_CIRCUIT_TASK_FACTORYTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBFactoryTask: public IBuilderTask {
public:
	CBFactoryTask(CCircuitAI* circuit, Priority priority,
				 springai::UnitDef* buildDef, const springai::AIFloat3& position,
				 BuildType type, float cost, int timeout);
	virtual ~CBFactoryTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FACTORYTASK_H_
