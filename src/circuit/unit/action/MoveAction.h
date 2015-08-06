/*
 * MoveAction.h
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_MOVEACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_MOVEACTION_H_

#include "unit/action/UnitAction.h"

#include "AIFloat3.h"

namespace circuit {

class CMoveAction: public IUnitAction {
public:
	enum class MoveType: char {RAW, SAFE};

	CMoveAction(CCircuitUnit* owner, const springai::AIFloat3& pos, MoveType mt = MoveType::SAFE);
	virtual ~CMoveAction();

	virtual void Update(CCircuitAI* circuit);
	virtual void OnStart();
	virtual void OnEnd();

private:
	springai::AIFloat3 position;
	MoveType moveType;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_MOVEACTION_H_
