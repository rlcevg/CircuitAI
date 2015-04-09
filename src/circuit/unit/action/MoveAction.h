/*
 * MoveAction.h
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_MOVEACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_MOVEACTION_H_

#include "unit/action/UnitAction.h"

namespace circuit {

class CMoveAction: public IUnitAction {
public:
	CMoveAction(CCircuitUnit* owner);
	virtual ~CMoveAction();

	virtual void Update(CCircuitAI* circuit);
	virtual void OnStart();
	virtual void OnEnd();
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_MOVEACTION_H_
