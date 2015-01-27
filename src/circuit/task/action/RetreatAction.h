/*
 * RetreatAction.h
 *
 *  Created on: Jan 27, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_ACTION_RETREATACTION_H_
#define SRC_CIRCUIT_TASK_ACTION_RETREATACTION_H_

#include "task/action/TaskAction.h"

namespace circuit {

class CRetreatAction: public ITaskAction {
public:
	CRetreatAction(IUnitTask* owner);
	virtual ~CRetreatAction();

	virtual void Update(CCircuitAI* circuit);
	virtual void OnStart();
	virtual void OnEnd();
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_ACTION_RETREATACTION_H_
