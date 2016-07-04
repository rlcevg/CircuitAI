/*
 * GuardAction.h
 *
 *  Created on: Jul 3, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_SUPPORTACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_SUPPORTACTION_H_

#include "unit/action/UnitAction.h"

namespace circuit {

class CSupportAction: public IUnitAction {
public:
	CSupportAction(CCircuitUnit* owner);
	virtual ~CSupportAction();

	virtual void Update(CCircuitAI* circuit);

private:
	unsigned int updCount;

};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_SUPPORTACTION_H_
