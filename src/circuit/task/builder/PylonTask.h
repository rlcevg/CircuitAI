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
	CBPylonTask(IUnitModule* mgr, Priority priority,
				CCircuitDef* buildDef, const springai::AIFloat3& position,
				IGridLink* link, SResource cost, int timeout);
	CBPylonTask(IUnitModule* mgr);  // Load
	virtual ~CBPylonTask();

	IGridLink* GetLink() { return link; }

protected:
	virtual void Finish() override;
	virtual void Cancel() override;

	virtual bool Execute(CCircuitUnit* unit) override;

private:
	IGridLink* link;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_PYLONTASK_H_
