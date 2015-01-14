/*
 * WaitAction.h
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_WAITACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_WAITACTION_H_

#include "unit/action/UnitAction.h"

namespace circuit {

class CWaitAction: public IUnitAction {
public:
	CWaitAction(CActionList* owner);
	virtual ~CWaitAction();

	virtual void Update(CCircuitAI* circuit);
	virtual void OnStart(void);
	virtual void OnEnd(void);

private:

};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_WAITACTION_H_
