/*
 * MoveAction.h
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_MOVEACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_MOVEACTION_H_

#include "unit/action/UnitAction.h"
#include "util/Defines.h"

namespace circuit {

class CMoveAction: public IUnitAction {
public:
	CMoveAction(CCircuitUnit* owner);
	CMoveAction(CCircuitUnit* owner, const F3Vec& path);
	virtual ~CMoveAction();

	virtual void Update(CCircuitAI* circuit);

	void SetPath(const F3Vec& path);

private:
	F3Vec path;
	int pathIterator;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_MOVEACTION_H_
