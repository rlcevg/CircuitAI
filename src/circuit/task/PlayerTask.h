/*
 * PlayerTask.h
 *
 *  Created on: Feb 3, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_PLAYERTASK_H_
#define SRC_CIRCUIT_TASK_PLAYERTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CPlayerTask: public IUnitTask {
public:
	CPlayerTask(ITaskManager* mgr);
	virtual ~CPlayerTask();

	virtual void Execute(CCircuitUnit* unit) override;
	virtual void Update() override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker) override;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_PLAYERTASK_H_
