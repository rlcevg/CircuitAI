/*
 * MilitaryManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "MilitaryManager.h"
#include "Circuit.h"
#include "Scheduler.h"
#include "utils.h"

#include "utils.h"
#include "Log.h"
#include "Unit.h"
#include "UnitDef.h"

#include <vector>

namespace circuit {

using namespace springai;

CMilitaryManager::CMilitaryManager(CCircuit* circuit) :
		IModule(circuit)
{
	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>(&CMilitaryManager::TestOrder, this), 120);
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
	return 0; //signaling: OK
}

void CMilitaryManager::TestOrder()
{
	circuit->LOG("HIT 120 frame");
	std::vector<Unit*> units = circuit->GetCallback()->GetTeamUnits();
	if (units.size() > 0) {
		circuit->LOG("found mah comm");
		Unit* commander = units.front();
		Unit* friendCommander = NULL;;
		std::vector<Unit*> friendlies = circuit->GetCallback()->GetFriendlyUnits();
		for (Unit* unit : friendlies) {
			UnitDef* unitDef = unit->GetDef();
			if (strcmp(unitDef->GetName(), "armcom1") == 0) {
				if (commander->GetUnitId() != unit->GetUnitId()) {
					circuit->LOG("found friendly comm");
					friendCommander = unit;
					break;
				} else {
					circuit->LOG("found mah comm again");
				}
			}
			delete unitDef;
		}

		if (friendCommander) {
			circuit->LOG("giving guard order");
			commander->Guard(friendCommander);
//			commander->Build(callback->GetUnitDefByName("armsolar"), commander->GetPos(), UNIT_COMMAND_BUILD_NO_FACING);
		}
		utils::FreeClear(friendlies);
	}
	utils::FreeClear(units);
}

} // namespace circuit
