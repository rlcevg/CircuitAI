/*
 * EconomyManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "EconomyManager.h"

namespace circuit
{

CEconomyManager::CEconomyManager(CCircuit* circuit) :
		IModule(circuit)
{
}

CEconomyManager::~CEconomyManager()
{
}

int CEconomyManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	return 0; //signaling: OK
}

int CEconomyManager::UnitFinished(CCircuitUnit* unit)
{
	return 0; //signaling: OK
}

} // namespace circuit
