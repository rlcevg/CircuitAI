/*
 * UnitAction.cpp
 *
 *  Created on: Jan 12, 2015
 *      Author: rlcevg
 */

#include "unit/action/UnitAction.h"

namespace circuit {

IUnitAction::IUnitAction(CActionList* owner, Type type) :
		IAction(owner),
		type(type)
{
}

IUnitAction::~IUnitAction()
{
}

} // namespace circuit
