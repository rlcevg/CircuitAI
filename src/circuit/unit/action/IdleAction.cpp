/*
 * IdleAction.cpp
 *
 *  Created on: Jan 15, 2015
 *      Author: rlcevg
 */

#include "unit/action/IdleAction.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "util/utils.h"

namespace circuit {

CIdleAction::CIdleAction(CCircuitUnit* owner) :
		IUnitAction(owner, Type::IDLE)
{
}

CIdleAction::~CIdleAction()
{
}

void CIdleAction::Update()
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	IUnitManager* manager = unit->GetManager();
	manager->AssignTask(unit);
	manager->ExecuteTask(unit);

	isFinished = true;
}

} // namespace circuit
