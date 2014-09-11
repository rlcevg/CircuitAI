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

#include "Log.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Map.h"

#include "AISCommands.h"

#include <vector>

namespace circuit {

using namespace springai;

CMilitaryManager::CMilitaryManager(CCircuitAI* circuit) :
		IModule(circuit)
{
	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>(&CMilitaryManager::TestOrder, this), 120);
	CGameAttribute* attrib = circuit->GetGameAttribute();
	int unitDefId;

	auto atackerFinishedHandler = [circuit](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		Map* map = circuit->GetMap();
		int terWidth = map->GetWidth() * SQUARE_SIZE;
		int terHeight = map->GetHeight() * SQUARE_SIZE;
		float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
		float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
		AIFloat3 fromPos(x, map->GetElevationAt(x, z), z);
		u->Fight(fromPos, UNIT_COMMAND_OPTION_SHIFT_KEY);

		x = rand() % (int)(terWidth + 1);
		z = rand() % (int)(terHeight + 1);
		AIFloat3 toPos(x, map->GetElevationAt(x, z), z);
		u->PatrolTo(toPos, UNIT_COMMAND_OPTION_SHIFT_KEY);
	};
	auto atackerIdleHandler = [circuit](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		Map* map = circuit->GetMap();
		int terWidth = map->GetWidth() * SQUARE_SIZE;
		int terHeight = map->GetHeight() * SQUARE_SIZE;
		float x = rand() % (int)(terWidth + 1);
		float z = rand() % (int)(terHeight + 1);
		AIFloat3 toPos(x, map->GetElevationAt(x, z), z);
		u->PatrolTo(toPos, 0);
	};

	unitDefId = attrib->GetUnitDefByName("armpw")->GetUnitDefId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
	unitDefId = attrib->GetUnitDefByName("armrock")->GetUnitDefId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
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
	auto search = finishedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetDef()->GetUnitDefId());
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
