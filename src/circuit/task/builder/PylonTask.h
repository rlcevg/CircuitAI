/*
 * PylonTask.h
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_PYLONTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_PYLONTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CEnergyLink;

class CBPylonTask: public IBuilderTask {
public:
	CBPylonTask(ITaskManager* mgr, Priority priority,
				CCircuitDef* buildDef, const springai::AIFloat3& position,
				CEnergyLink* link, float cost, int timeout);
	virtual ~CBPylonTask();

	CEnergyLink* GetLink() { return link; }

	virtual void Execute(CCircuitUnit* unit);
protected:
	virtual void Finish();
	virtual void Cancel();

private:
	CEnergyLink* link;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_PYLONTASK_H_
