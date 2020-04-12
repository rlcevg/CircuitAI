/*
 * IdleAction.cpp
 *
 *  Created on: Jul 30, 2015
 *      Author: rlcevg
 */

#include "unit/action/IdleAction.h"
#include "util/Utils.h"

namespace circuit {

CIdleAction::CIdleAction(CCircuitUnit* owner)
		: IUnitAction(owner, Type::IDLE)
{
}

CIdleAction::~CIdleAction()
{
}

void CIdleAction::Update(CCircuitAI* circuit)
{
}

} // namespace circuit
