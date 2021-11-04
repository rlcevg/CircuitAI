/*
 * CaptureAction.h
 *
 *  Created on: Oct 31, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_CAPTUREACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_CAPTUREACTION_H_

#include "unit/action/UnitAction.h"

namespace circuit {

class CCaptureAction: public IUnitAction {
public:
	CCaptureAction(CCircuitUnit* owner, float range);
	virtual ~CCaptureAction();

	virtual void Update(CCircuitAI* circuit) override;

private:
	float range;
	unsigned int updCount;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_CAPTUREACTION_H_
