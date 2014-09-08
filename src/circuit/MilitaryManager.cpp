/*
 * MilitaryManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "MilitaryManager.h"
#include "CircuitAI.h"
#include "GameAttribute.h"
#include "Scheduler.h"
#include "CircuitUnit.h"
#include "utils.h"

#include "utils.h"
#include "Log.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Map.h"

#include <vector>

namespace circuit {

using namespace springai;

CMilitaryManager::CMilitaryManager(CCircuitAI* circuit) :
		IModule(circuit)
{
	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>(&CMilitaryManager::TestOrder, this), 120);
	CGameAttribute* attrib = circuit->GetGameAttribute();
	int unitDefId;

	unitDefId = attrib->GetUnitDefByName("armpw")->GetUnitDefId();
	finishedHandler[unitDefId] = [circuit](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		int width = circuit->GetMap()->GetWidth() * SQUARE_SIZE;
		int height = circuit->GetMap()->GetHeight() * SQUARE_SIZE;
		AIFloat3 fromPos(0 + (rand() % (int)(width - 0 + 1)), 0, 0 + (rand() % (int)(height - 0 + 1)));
		u->Fight(fromPos, 0);
	};

	idleHandler[unitDefId] = [circuit](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		int width = circuit->GetMap()->GetWidth() * SQUARE_SIZE;
		int height = circuit->GetMap()->GetHeight() * SQUARE_SIZE;
		AIFloat3 toPos(0 + (rand() % (int)(width - 0 + 1)), 0, 0 + (rand() % (int)(height - 0 + 1)));
		u->PatrolTo(toPos, 0);
	};
}

CMilitaryManager::~CMilitaryManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

int CMilitaryManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	return 0; //signaling: OK
}

int CMilitaryManager::UnitFinished(CCircuitUnit* unit)
{
	UnitDef* def = unit->GetDef();
	auto search = finishedHandler.find(def->GetUnitDefId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitIdle(CCircuitUnit* unit)
{
	UnitDef* def = unit->GetDef();
	auto search = idleHandler.find(def->GetUnitDefId());
	if (search != idleHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

void CMilitaryManager::TestOrder()
{
	circuit->LOG("HIT 120 frame");
//	std::vector<Unit*> units = circuit->GetCallback()->GetTeamUnits();
//	if (!units.empty()) {
//		circuit->LOG("found mah comm");
//		Unit* commander = units.front();
//		Unit* friendCommander = NULL;;
//		std::vector<Unit*> friendlies = circuit->GetCallback()->GetFriendlyUnits();
//		for (Unit* unit : friendlies) {
//			UnitDef* unitDef = unit->GetDef();
//			if (strcmp(unitDef->GetName(), "armcom1") == 0) {
//				if (commander->GetUnitId() != unit->GetUnitId()) {
//					circuit->LOG("found friendly comm");
//					friendCommander = unit;
//					break;
//				} else {
//					circuit->LOG("found mah comm again");
//				}
//			}
//			delete unitDef;
//		}
//
//		if (friendCommander) {
//			circuit->LOG("giving guard order");
//			commander->Guard(friendCommander);
////			commander->Build(callback->GetUnitDefByName("armsolar"), commander->GetPos(), UNIT_COMMAND_BUILD_NO_FACING);
//		}
//		utils::FreeClear(friendlies);
//	}
//	utils::FreeClear(units);
}

} // namespace circuit
