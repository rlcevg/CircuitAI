/*
 * MoveAction.h
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_MOVEACTION_H_
#define SRC_CIRCUIT_TASK_MOVEACTION_H_

#include "unit/action/UnitAction.h"

namespace circuit {

class CMoveAction: public IUnitAction {
public:
	CMoveAction(CActionList* owner);
	virtual ~CMoveAction();

	virtual void Update(CCircuitAI* circuit);
	virtual void OnStart(void);
	virtual void OnEnd(void);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_MOVEACTION_H_
