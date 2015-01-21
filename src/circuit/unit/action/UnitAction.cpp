/*
 * UnitAction.cpp
 *
 *  Created on: Jan 12, 2015
 *      Author: rlcevg
 */

#include "unit/action/UnitAction.h"
#include "unit/CircuitUnit.h"

namespace circuit {

IUnitAction::IUnitAction(CCircuitUnit* owner, Type type) :
		IAction(owner),
		type(type)
{
}

IUnitAction::~IUnitAction()
{
}

IUnitAction::Type IUnitAction::GetType()
{
	return type;
}

} // namespace circuit
