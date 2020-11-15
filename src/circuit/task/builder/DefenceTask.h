/*
 * DefenceTask.h
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_DEFENCETASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_DEFENCETASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBDefenceTask: public IBuilderTask {
public:
	CBDefenceTask(ITaskManager* mgr, Priority priority,
				  CCircuitDef* buildDef, const springai::AIFloat3& position,
				  float cost, float shake, int timeout);
	virtual ~CBDefenceTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void Update() override;

protected:
	virtual void Finish() override;
	virtual void Cancel() override;

	bool isUrgent;
	float normalCost;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_DEFENCETASK_H_
