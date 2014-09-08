/*
 * EconomyManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "EconomyManager.h"
#include "CircuitAI.h"
#include "GameAttribute.h"
#include "CircuitUnit.h"
#include "utils.h"

#include "Unit.h"
#include "UnitDef.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CEconomyManager::CEconomyManager(CCircuitAI* circuit) :
		IModule(circuit),
		totalBuildpower(.0f)
{
	CGameAttribute* attrib = circuit->GetGameAttribute();
	int unitDefId;

	unitDefId = attrib->GetUnitDefByName("factorycloak")->GetUnitDefId();
	finishedHandler[unitDefId] = [circuit](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		u->SetRepeat(true, 0);
		AIFloat3 buildPos(0, 0, 0);
		UnitDef* def = circuit->GetGameAttribute()->GetUnitDefByName("armpw");
		u->Build(def, buildPos, 0, 0);
	};
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
	printf("%s | %s | %s\n", def->GetHumanName(), def->GetName(), def->GetWreckName());
	delete def;

	if (builder == nullptr) {
		printf("CCircuitUnit == nullptr\n");
		return 3;
	}
	u = builder->GetUnit();
	if (u == nullptr) {
		printf("Unit == nullptr\n");
		return 4;
	}
	def = u->GetDef();
	printf("%s | %s | %s\n", def->GetHumanName(), def->GetName(), def->GetWreckName());
	delete def;

	// check for assisters
	return 0; //signaling: OK
}

int CEconomyManager::UnitFinished(CCircuitUnit* unit)
{
	UnitDef* def = unit->GetDef();
	auto search = finishedHandler.find(def->GetUnitDefId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	if (def->IsBuilder()) {
		if (!def->GetBuildOptions().empty()) {
			workers.insert(unit);
		} else {
			// Nanos? Make them work
			Unit* u =unit->GetUnit();
			AIFloat3 toPos = u->GetPos();
			float size = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE;
			toPos.x += (toPos.x > circuit->GetMap()->GetWidth() * SQUARE_SIZE / 2) ? -size : size;
			toPos.z += (toPos.z > circuit->GetMap()->GetHeight() * SQUARE_SIZE / 2) ? -size : size;
			u->SetRepeat(true, 0);  // not necessary, maybe for later use
			// Use u->Fight(toPos, 0) when AI is in same team as human or disable "Auto Patrol Nanos" widget
			u->PatrolTo(toPos, 0);
		}
		totalBuildpower += def->GetBuildSpeed();
	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	auto search = workers.find(unit);
	if (search != workers.end()) {
		UnitDef* def = unit->GetDef();
		totalBuildpower -= def->GetBuildSpeed();
		workers.erase(search);
	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId)
{
	UnitFinished(unit);
	return 0; //signaling: OK
}

int CEconomyManager::UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId)
{
	UnitDestroyed(unit, nullptr);
	return 0; //signaling: OK
}

} // namespace circuit
