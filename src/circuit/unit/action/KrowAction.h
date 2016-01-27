/*
 * KrowAction.h
 *
 *  Created on: Jan 26, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_KROWACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_KROWACTION_H_

#include "unit/action/UnitAction.h"

namespace circuit {

class CKrowAction: public IUnitAction {
public:
	CKrowAction(CCircuitUnit* owner);
	virtual ~CKrowAction();
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_KROWACTION_H_
