/*
 * ResurrectTask.h
 *
 *  Created on: Apr 16, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_RESURRECTTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_RESURRECTTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBResurrectTask: public IBuilderTask {
public:
	CBResurrectTask(IUnitModule* mgr, Priority priority, const springai::AIFloat3& position,
					SResource cost, int timeout, float radius = .0f);
	CBResurrectTask(IUnitModule* mgr);  // Load
	virtual ~CBResurrectTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;  // FIXME: Remove when proper task assignment implemented

private:
	virtual void Finish() override;
	virtual void Cancel() override;

	virtual bool Execute(CCircuitUnit* unit) override;

	virtual bool Reevaluate(CCircuitUnit* unit) override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;

	bool IsInRange(const springai::AIFloat3& pos, float range) const;

private:
	virtual bool Load(std::istream& is) override;
	virtual void Save(std::ostream& os) const override;

	float radius;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_RESURRECTTASK_H_
