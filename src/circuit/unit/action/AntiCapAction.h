/*
 * AntiCapAction.h
 *
 *  Created on: Mar 5, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_ANTICAPACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_ANTICAPACTION_H_

#include "unit/action/UnitAction.h"

namespace circuit {

class CAntiCapAction: public IUnitAction {
public:
	CAntiCapAction(CCircuitUnit* owner);
	virtual ~CAntiCapAction();

	virtual void Update(CCircuitAI* circuit) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_ANTICAPACTION_H_
