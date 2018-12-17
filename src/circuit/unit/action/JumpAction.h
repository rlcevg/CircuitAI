/*
 * JumpAction.h
 *
 *  Created on: Jan 26, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_JUMPACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_JUMPACTION_H_

#include "unit/action/TravelAction.h"

namespace circuit {

class CJumpAction: public ITravelAction {
public:
	CJumpAction(CCircuitUnit* owner, int squareSize, float speed = NO_SPEED_LIMIT);
	CJumpAction(CCircuitUnit* owner, const std::shared_ptr<F3Vec>& pPath, int squareSize, float speed = NO_SPEED_LIMIT);
	virtual ~CJumpAction();

	virtual void Update(CCircuitAI* circuit) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_JUMPACTION_H_
