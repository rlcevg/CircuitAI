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
	CBBigGunTask(ITaskManager* mgr, Priority priority,
				 CCircuitDef* buildDef, const springai::AIFloat3& position,
				 float cost, bool isShake, int timeout);
	virtual ~CBBigGunTask();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_BIGGUNTASK_H_
