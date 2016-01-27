/*
 * FightAction.h
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_FIGHTACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_FIGHTACTION_H_

#include "unit/action/UnitAction.h"
#include "util/Defines.h"

#include <memory>

namespace circuit {

class CFightAction: public IUnitAction {
public:
	CFightAction(CCircuitUnit* owner, float speed = MAX_SPEED);
	CFightAction(CCircuitUnit* owner, const std::shared_ptr<F3Vec>& pPath, float speed = MAX_SPEED);
	virtual ~CFightAction();

	virtual void Update(CCircuitAI* circuit);

	void SetPath(const std::shared_ptr<F3Vec>& pPath, float speed = MAX_SPEED);

private:
	std::shared_ptr<F3Vec> pPath;
	float speed;
	int pathIterator;
	int increment;
	int minSqDist;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_FIGHTACTION_H_
