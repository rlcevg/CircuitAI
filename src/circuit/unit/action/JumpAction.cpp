/*
 * JumpAction.cpp
 *
 *  Created on: Jan 26, 2016
 *      Author: rlcevg
 */

#include "unit/action/JumpAction.h"

namespace circuit {

CJumpAction::CJumpAction(CCircuitUnit* owner)
		: IUnitAction(owner, Type::JUMP)
{
	// TODO Auto-generated constructor stub

}

CJumpAction::~CJumpAction()
{
	// TODO Auto-generated destructor stub
}

} // namespace circuit
