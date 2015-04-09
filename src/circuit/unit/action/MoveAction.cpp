/*
 * MoveAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/MoveAction.h"
#include "util/utils.h"

namespace circuit {

CMoveAction::CMoveAction(CCircuitUnit* owner) :
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

void CMoveAction::OnStart()
{

}

void CMoveAction::OnEnd()
{

}

} // namespace circuit
