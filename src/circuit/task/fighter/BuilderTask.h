/*
 * BuilderTask.h
 *
 *  Created on: Apr 10, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_BUILDERTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_BUILDERTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CBuilderTask: public IFighterTask {
public:
	CBuilderTask(ITaskManager* mgr, float powerMod);
	virtual ~CBuilderTask();

	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;

private:
	void Execute(CCircuitUnit* unit, bool isUpdating);
	CEnemyInfo* FindTarget(CCircuitUnit* unit, const springai::AIFloat3& pos);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_BUILDERTASK_H_
