/*
 * StaticReclaim.h
 *
 *  Created on: Mar 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_STATICRECLAIM_H_
#define SRC_CIRCUIT_TASK_BUILDER_STATICRECLAIM_H_

#include "task/builder/ReclaimTask.h"

namespace circuit {

class CStaticReclaim: public CBReclaimTask {
public:
	CStaticReclaim(ITaskManager* mgr, Priority priority,
				   springai::UnitDef* buildDef, const springai::AIFloat3& position,
				   float cost, int timeout, float radius = .0f);
	virtual ~CStaticReclaim();

	virtual void OnUnitIdle(CCircuitUnit* unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_STATICRECLAIM_H_
