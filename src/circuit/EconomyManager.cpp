/*
 * EconomyManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "EconomyManager.h"
#include "CircuitUnit.h"
#include "utils.h"

#include "Unit.h"
#include "UnitDef.h"

namespace circuit {

using namespace springai;

CEconomyManager::CEconomyManager(CCircuit* circuit) :
		IModule(circuit)
{
}

CEconomyManager::~CEconomyManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

int CEconomyManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	PRINT_DEBUG("%s\n", __PRETTY_FUNCTION__);
	if (unit == nullptr) {
		printf("CCircuitUnit == nullptr\n");
		return 1;
	}
	Unit* u = unit->GetUnit();
	if (u == nullptr) {
		printf("Unit == nullptr\n");
		return 2;
	}
	UnitDef* def = u->GetDef();
	printf("%s\n", def->GetHumanName());
	printf("%s\n", def->GetName());
	printf("%s\n", def->GetWreckName());
	delete def;

	if (builder == nullptr) {
		printf("CCircuitUnit == nullptr\n");
		return 1;
	}
	u = builder->GetUnit();
	if (u == nullptr) {
		printf("Unit == nullptr\n");
		return 2;
	}
	def = u->GetDef();
	printf("%s\n", def->GetHumanName());
	printf("%s\n", def->GetName());
	printf("%s\n", def->GetWreckName());
	delete def;

	// check for assisters
	return 0; //signaling: OK
}

int CEconomyManager::UnitFinished(CCircuitUnit* unit)
{
	PRINT_DEBUG("%s\n", __PRETTY_FUNCTION__);
	if (unit == nullptr) {
		printf("CCircuitUnit == nullptr\n");
		return 1;
	}
	Unit* u = unit->GetUnit();
	if (u == nullptr) {
		printf("Unit == nullptr\n");
		return 2;
	}
	UnitDef* def = u->GetDef();
	printf("%s\n", def->GetHumanName());
	printf("%s\n", def->GetName());
	printf("%s\n", def->GetWreckName());
	delete def;

	return 0; //signaling: OK
}

int CEconomyManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	// Reclaimed unit doesn't generate this event
	return 0; //signaling: OK
}

} // namespace circuit
