/*
 * MilitaryTask.h
 *
 *  Created on: Apr 10, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_COMBATTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_COMBATTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CCombatTask: public IFighterTask {
public:
	CCombatTask(ITaskManager* mgr, float powerMod);
	virtual ~CCombatTask();

	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;

	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;

private:
	void Execute(CCircuitUnit* unit);
	CEnemyInfo* FindTarget(CCircuitUnit* unit, const springai::AIFloat3& pos);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_COMBATTASK_H_
