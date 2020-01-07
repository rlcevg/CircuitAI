/*
 * ReclaimTask.h
 *
 *  Created on: Mar 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_STATIC_RECLAIMTASK_H_
#define SRC_CIRCUIT_TASK_STATIC_RECLAIMTASK_H_

#include "task/common/ReclaimTask.h"

namespace circuit {

class CSReclaimTask: public IReclaimTask {
public:
	CSReclaimTask(ITaskManager* mgr, Priority priority,
				  const springai::AIFloat3& position,
				  float cost, int timeout, float radius = .0f);
	virtual ~CSReclaimTask();

	virtual void AssignTo(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;

	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_STATIC_RECLAIMTASK_H_
