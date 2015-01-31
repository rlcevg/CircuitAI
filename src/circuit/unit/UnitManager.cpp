/*
 * UnitManager.cpp
 *
 *  Created on: Jan 15, 2015
 *      Author: rlcevg
 */

#include "unit/UnitManager.h"
#include "CircuitAI.h"
#include "task/IdleTask.h"
#include "task/RetreatTask.h"

namespace circuit {

IUnitManager::IUnitManager(CCircuitAI* circuit)
{
	idleTask = new CIdleTask(circuit);
	retreatTask = new CRetreatTask(circuit);
}

IUnitManager::~IUnitManager()
{
	delete idleTask, retreatTask;
}

void IUnitManager::FallbackTask(CCircuitUnit* unit)
{
}

CIdleTask* IUnitManager::GetIdleTask()
{
	return idleTask;
}

CRetreatTask* IUnitManager::GetRetreatTask()
{
	return retreatTask;
}

} // namespace circuit
