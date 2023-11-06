/*
 * NanoTask.h
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_NANOTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_NANOTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBNanoTask: public IBuilderTask {
public:
	CBNanoTask(IUnitModule* mgr, Priority priority,
			   CCircuitDef* buildDef, const springai::AIFloat3& position,
			   SResource cost, float shake, int timeout);
	CBNanoTask(IUnitModule* mgr);  // Load
	virtual ~CBNanoTask();

protected:
	virtual bool Execute(CCircuitUnit* unit) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_NANOTASK_H_
