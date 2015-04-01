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
				  springai::UnitDef* buildDef, const springai::AIFloat3& position,
				  float cost, int timeout);
	virtual ~CBDefenceTask();

protected:
	virtual void Finish();
	virtual void Cancel();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_DEFENCETASK_H_
