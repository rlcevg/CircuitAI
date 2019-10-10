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

class IGridLink;

class CBPylonTask: public IBuilderTask {
public:
	CBPylonTask(ITaskManager* mgr, Priority priority,
				CCircuitDef* buildDef, const springai::AIFloat3& position,
				IGridLink* link, float cost, int timeout);
	virtual ~CBPylonTask();

	IGridLink* GetLink() { return link; }

	virtual void Execute(CCircuitUnit* unit) override;
protected:
	virtual void Finish() override;
	virtual void Cancel() override;

private:
	IGridLink* link;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_PYLONTASK_H_
