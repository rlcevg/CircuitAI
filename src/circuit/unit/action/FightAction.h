/*
 * FightAction.h
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_FIGHTACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_FIGHTACTION_H_

#include "unit/action/TravelAction.h"

namespace circuit {

class CFightAction: public ITravelAction {
public:
	CFightAction(CCircuitUnit* owner, int squareSize, float speed = NO_SPEED_LIMIT);
	CFightAction(CCircuitUnit* owner, const std::shared_ptr<F3Vec>& pPath, int squareSize, float speed = NO_SPEED_LIMIT);
	virtual ~CFightAction();

	virtual void Update(CCircuitAI* circuit) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_FIGHTACTION_H_
