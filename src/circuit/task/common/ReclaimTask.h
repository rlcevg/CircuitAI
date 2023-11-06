/*
 * ReclaimTask.h
 *
 *  Created on: Sep 4, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_COMMON_RECLAIMTASK_H_
#define SRC_CIRCUIT_TASK_COMMON_RECLAIMTASK_H_

#include "task/builder/BuilderTask.h"
#include "unit/CircuitUnit.h"

namespace circuit {

class IReclaimTask: public IBuilderTask {
public:
	IReclaimTask(IUnitModule* mgr, Priority priority, Type type,
				 const springai::AIFloat3& position,
				 SResource cost, int timeout, float radius = .0f, bool isMetal = true);
	IReclaimTask(IUnitModule* mgr, Priority priority, Type type,
				 CCircuitUnit* target,
				 int timeout);
	IReclaimTask(IUnitModule* mgr, Type type);  // Load
	virtual ~IReclaimTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override = 0;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;  // FIXME: Remove when proper task assignment implemented

protected:
	virtual void Finish() override;
	virtual void Cancel() override;

	virtual bool Execute(CCircuitUnit* unit) override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;

	virtual void SetTarget(CCircuitUnit* unit) override;
	bool IsInRange(const springai::AIFloat3& pos, float range) const;

protected:
	virtual bool Load(std::istream& is) override;
	virtual void Save(std::ostream& os) const override;

	float radius;
	bool isMetal;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_COMMON_RECLAIMTASK_H_
