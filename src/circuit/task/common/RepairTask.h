/*
 * RepairTask.h
 *
 *  Created on: Sep 4, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_COMMON_REPAIRTASK_H_
#define SRC_CIRCUIT_TASK_COMMON_REPAIRTASK_H_

#include "task/builder/BuilderTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

class IRepairTask: public IBuilderTask {
public:
	IRepairTask(IUnitModule* mgr, Priority priority, Type type, CAllyUnit* target, int timeout = 0);
	IRepairTask(IUnitModule* mgr, Type type);  // Load
	virtual ~IRepairTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
protected:
	virtual void Finish() override;
	virtual void Cancel() override final;

	virtual bool Execute(CCircuitUnit* unit) override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override = 0;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override = 0;

	virtual void SetTarget(CCircuitUnit* unit) { SetRepTarget(unit); }
	ICoreUnit::Id GetTargetId() const { return targetId; }

protected:
	CAllyUnit* FindUnitToAssist(CCircuitUnit* unit);

	void SetRepTarget(CAllyUnit* unit);

	virtual bool Load(std::istream& is) override;
	virtual void Save(std::ostream& os) const override;

	ICoreUnit::Id targetId;  // Ignore "target" variable because ally units are vague
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_COMMON_REPAIRTASK_H_
