/*
 * WaitAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/WaitAction.h"
#include "util/Utils.h"

namespace circuit {

CWaitAction::CWaitAction(CCircuitUnit* owner)
		: IUnitAction(owner, Type::WAIT)
{
}

CWaitAction::~CWaitAction()
{
}

void CWaitAction::Update(CCircuitAI* circuit)
{
}

void CWaitAction::OnStart()
{
}

void CWaitAction::OnEnd()
{
}

} // namespace circuit
