/*
 * IdleAction.cpp
 *
 *  Created on: Jul 30, 2015
 *      Author: rlcevg
 */

#include "unit/action/IdleAction.h"
#include "util/utils.h"

namespace circuit {

CIdleAction::CIdleAction(CCircuitUnit* owner)
		: IUnitAction(owner, Type::IDLE)
{
}

CIdleAction::~CIdleAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CIdleAction::Update(CCircuitAI* circuit)
{
}

} // namespace circuit
