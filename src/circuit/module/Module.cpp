/*
 * Module.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "module/Module.h"
#include "script/Script.h"
#include "unit/CircuitUnit.h"

namespace circuit {

IModule::IModule(CCircuitAI* circuit, IScript* script)
		: circuit(circuit)
		, script(script)
{
}

IModule::~IModule()
{
	delete script;
}

bool IModule::InitScript()
{
	return script->Init();
}

int IModule::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	return 0; //signaling: OK
}

int IModule::UnitFinished(CCircuitUnit* unit)
{
	return 0; //signaling: OK
}

int IModule::UnitIdle(CCircuitUnit* unit)
{
	return 0; //signaling: OK
}

int IModule::UnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	return 0; //signaling: OK
}

int IModule::UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	return 0; //signaling: OK
}

int IModule::UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId)
{
	UnitCreated(unit, nullptr);
	if (!unit->GetUnit()->IsBeingBuilt()) {
		UnitFinished(unit);
//		UnitIdle(unit);
	}
	return 0; //signaling: OK
}

int IModule::UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId)
{
	UnitDestroyed(unit, nullptr);
	return 0; //signaling: OK
}

} // namespace circuit
