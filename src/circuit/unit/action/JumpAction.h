/*
 * JumpAction.h
 *
 *  Created on: Jan 26, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_JUMPACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_JUMPACTION_H_

#include "unit/action/UnitAction.h"

namespace circuit {

class CJumpAction: public IUnitAction {
public:
	CJumpAction(CCircuitUnit* owner);
	virtual ~CJumpAction();
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_JUMPACTION_H_
