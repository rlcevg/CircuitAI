/*
 * MoveAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/MoveAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

CMoveAction::CMoveAction(CActionList* owner) :
		IUnitAction(owner, Type::MOVE)
{
}

CMoveAction::~CMoveAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CMoveAction::Update(CCircuitAI* circuit)
{

}

void CMoveAction::OnStart(void)
{

}

void CMoveAction::OnEnd(void)
{

}

} // namespace circuit
