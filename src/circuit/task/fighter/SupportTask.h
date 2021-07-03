/*
 * SupportTask.h
 *
 *  Created on: Jul 3, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_SUPPORTTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_SUPPORTTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CSupportTask: public IFighterTask {
public:
	CSupportTask(ITaskManager* mgr);
	virtual ~CSupportTask();

	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;

private:
	void ApplyPath(const CQueryPathMulti* query);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_SUPPORTTASK_H_
