/*
 * IdleAction.h
 *
 *  Created on: Jul 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_IDLEACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_IDLEACTION_H_

#include "unit/action/UnitAction.h"

namespace circuit {

class CIdleAction: public IUnitAction {
public:
	CIdleAction(CCircuitUnit* owner);
	virtual ~CIdleAction();

	virtual void Update(CCircuitAI* circuit) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_IDLEACTION_H_
