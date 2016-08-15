/*
 * SuperTask.h
 *
 *  Created on: Aug 12, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_STATIC_SUPERTASK_H_
#define SRC_CIRCUIT_TASK_STATIC_SUPERTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CSuperTask: public IFighterTask {
public:
	CSuperTask(ITaskManager* mgr);
	virtual ~CSuperTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

private:
	int targetFrame;
	bool isAttack;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_STATIC_SUPERTASK_H_
