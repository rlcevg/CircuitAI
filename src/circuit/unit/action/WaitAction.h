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
	CWaitAction(CCircuitUnit* owner);
	virtual ~CWaitAction();

	virtual void Update(CCircuitAI* circuit);
	virtual void OnStart();
	virtual void OnEnd();
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_WAITACTION_H_
