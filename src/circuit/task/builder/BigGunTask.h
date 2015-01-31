/*
 * BigGunTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_BIGGUNTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_BIGGUNTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBBigGunTask: public IBuilderTask {
public:
	CBBigGunTask(CCircuitAI* circuit, Priority priority,
				 springai::UnitDef* buildDef, const springai::AIFloat3& position,
				 BuildType type, float cost, int timeout);
	virtual ~CBBigGunTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_BIGGUNTASK_H_
