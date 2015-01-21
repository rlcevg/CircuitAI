/*
 * WaitAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/WaitAction.h"
#include "util/utils.h"

namespace circuit {

CWaitAction::CWaitAction(CCircuitUnit* owner) :
		IUnitAction(owner, Type::WAIT)
{
}

CWaitAction::~CWaitAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CWaitAction::Update()
{

}

void CWaitAction::OnStart()
{

}

void CWaitAction::OnEnd()
{

}

} // namespace circuit
