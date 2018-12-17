/*
 * RetreatTask.h
 *
 *  Created on: Jan 18, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_RETREATTASK_H_
#define SRC_CIRCUIT_TASK_RETREATTASK_H_

#include "task/UnitTask.h"
#include "util/Defines.h"

namespace circuit {

class CRetreatTask: public IUnitTask {
public:
	CRetreatTask(ITaskManager* mgr, int timeout = ASSIGN_TIMEOUT);
	virtual ~CRetreatTask();

	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Execute(CCircuitUnit* unit) override;
	virtual void Update() override;
protected:
	virtual void Finish() override;
	virtual void Cancel() override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker) override;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker) override;

	void CheckRepairer(CCircuitUnit* unit);
	void SetRepairer(CCircuitUnit* unit) { repairer = unit; }
	CCircuitUnit* GetRepairer() const { return repairer; }

private:
	CCircuitUnit* repairer;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_RETREATTASK_H_
