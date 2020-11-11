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

class CResurrectTask: public IBuilderTask {
public:
	CResurrectTask(ITaskManager* mgr, Priority priority, const springai::AIFloat3& position,
				   float cost, int timeout, float radius = .0f);
	virtual ~CResurrectTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;  // FIXME: Remove when proper task assignment implemented

private:
	virtual void Finish() override;
	virtual void Cancel() override;

	virtual void Execute(CCircuitUnit* unit) override;

	virtual bool Reevaluate(CCircuitUnit* unit) override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;

	bool IsInRange(const springai::AIFloat3& pos, float range) const;

private:
	float radius;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_RESURRECTTASK_H_
