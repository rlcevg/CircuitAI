/*
 * StoreTask.h
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_STORETASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_STORETASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBStoreTask: public IBuilderTask {
public:
	CBStoreTask(CCircuitAI* circuit, Priority priority,
				springai::UnitDef* buildDef, const springai::AIFloat3& position,
				float cost, int timeout);
	virtual ~CBStoreTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_STORETASK_H_
