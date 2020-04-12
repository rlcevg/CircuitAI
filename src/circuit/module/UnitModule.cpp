/*
 * UnitModule.cpp
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#include "module/UnitModule.h"

namespace circuit {

IUnitModule::IUnitModule(CCircuitAI* circuit, IScript* script)
		: IModule(circuit, script)
		, IUnitManager()
		, ITaskManager()
{
	Init();
}

IUnitModule::~IUnitModule()
{
}

CCircuitAI* IUnitModule::GetCircuit()
{
	return circuit;
}

} // namespace circuit
