/*
 * MilitaryManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "module/MilitaryManager.h"
#include "CircuitAI.h"
#include "unit/CircuitUnit.h"
#include "static/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "Log.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Map.h"
// debug
#include "Pathing.h"
#include "Drawer.h"
#include "Game.h"
#include "OOAICallback.h"

#include "AISCommands.h"

#include <vector>

namespace circuit {

using namespace springai;

CMilitaryManager::CMilitaryManager(CCircuitAI* circuit) :
		IModule(circuit)
{
	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>(&CMilitaryManager::TestOrder, this), 120);
	int unitDefId;

	auto atackerFinishedHandler = [this](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		CTerrainManager* terrain = this->circuit->GetTerrainManager();
		int terWidth = terrain->GetTerrainWidth();
		int terHeight = terrain->GetTerrainHeight();
		float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
		float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
		AIFloat3 fromPos(x, this->circuit->GetMap()->GetElevationAt(x, z), z);
		u->Fight(fromPos, UNIT_COMMAND_OPTION_SHIFT_KEY, FRAMES_PER_SEC * 60);

		std::vector<float> params;
		params.push_back(2);
		u->ExecuteCustomCommand(CMD_RETREAT, params);
	};
	auto atackerIdleHandler = [this](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		CTerrainManager* terrain = this->circuit->GetTerrainManager();
		int terWidth = terrain->GetTerrainWidth();
		int terHeight = terrain->GetTerrainHeight();
		float x = rand() % (int)(terWidth + 1);
		float z = rand() % (int)(terHeight + 1);
		AIFloat3 toPos(x, this->circuit->GetMap()->GetElevationAt(x, z), z);
//		u->PatrolTo(toPos);
		u->Fight(toPos, 0, FRAMES_PER_SEC * 60 * 5);
	};

	unitDefId = circuit->GetUnitDefByName("armpw")->GetUnitDefId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
	unitDefId = circuit->GetUnitDefByName("armrock")->GetUnitDefId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
	unitDefId = circuit->GetUnitDefByName("armwar")->GetUnitDefId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
	unitDefId = circuit->GetUnitDefByName("armzeus")->GetUnitDefId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
	unitDefId = circuit->GetUnitDefByName("armjeth")->GetUnitDefId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;
	unitDefId = circuit->GetUnitDefByName("armsnipe")->GetUnitDefId();
	finishedHandler[unitDefId] = atackerFinishedHandler;
	idleHandler[unitDefId] = atackerIdleHandler;

	/*
	 * raveparty handlers
	 */
	finishedHandler[circuit->GetUnitDefByName("raveparty")->GetUnitDefId()] = [this](CCircuitUnit* unit) {
		unit->GetUnit()->SetTrajectory(1);
	};

//	/*
//	 * armrectr handlers
//	 */
//	unitDefId = attrib->GetUnitDefByName("armrectr")->GetUnitDefId();
//	idleHandler[unitDefId] = [this](CCircuitUnit* unit) {
//		fighterInfos.erase(unit);
//	};
//	damagedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
//		if (attacker != nullptr) {
//			auto search = fighterInfos.find(unit);
//			if (search == fighterInfos.end()) {
//				Unit* u = unit->GetUnit();
//				std::vector<float> params;
//				params.push_back(2.0f);
//				u->ExecuteCustomCommand(CMD_PRIORITY, params);
//
//				const AIFloat3& pos = attacker->GetUnit()->GetPos();
//				params.clear();
//				params.push_back(1.0f);  // 1: terraform_type, 1 == level
//				params.push_back(this->circuit->GetTeamId());  // 2: teamId
//				params.push_back(0.0f);  // 3: terraform type - 0 == Wall, else == Area
//				params.push_back(pos.y - 42.0f);  // 4: terraformHeight
//				params.push_back(1.0f);  // 5: number of control points
//				params.push_back(1.0f);  // 6: units count?
//				params.push_back(0.0f);  // 7: volumeSelection?
//				params.push_back(pos.x);  //  8: i + 0 control point x
//				params.push_back(pos.z);  //  9: i + 1 control point z
//				params.push_back(u->GetUnitId());  // 10: i + 2 unitId
//				u->ExecuteCustomCommand(CMD_TERRAFORM_INTERNAL, params);
//
//				fighterInfos[unit].isTerraforming = true;
//			}
//		}
//	};
//	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
//		fighterInfos.erase(unit);
//	};
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

//int CMilitaryManager::UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
//{
//	auto search = damagedHandler.find(unit->GetDef()->GetUnitDefId());
//	if (search != damagedHandler.end()) {
//		search->second(unit, attacker);
//	}
//
//	return 0; //signaling: OK
//}
//
//int CMilitaryManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
//{
//	auto search = destroyedHandler.find(unit->GetDef()->GetUnitDefId());
//	if (search != destroyedHandler.end()) {
//		search->second(unit, attacker);
//	}
//
//	return 0; //signaling: OK
//}

int CMilitaryManager::EnemyEnterLOS(CCircuitUnit* unit)
{
	// debug
//	if (strcmp(unit->GetDef()->GetName(), "factorycloak") == 0) {
//		circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>([this](CCircuitUnit* unit) {
//			Unit* u = unit->GetUnit();
//			Pathing* pathing = circuit->GetPathing();
//			Map* map = circuit->GetMap();
//			const CMetalManager::Metals& spots = circuit->GetMetalManager().GetSpots();
//			const AIFloat3& start = u->GetPos();
//			for (auto& s : spots) {
//				AIFloat3 end = s.position;
//				int pathId = pathing->InitPath(start, end, 4, .0f);
//				AIFloat3 lastPoint, point(start);
//				Drawer* drawer = map->GetDrawer();
//				do {
//					lastPoint = point;
//					point = pathing->GetNextWaypoint(pathId);
//					drawer->AddLine(lastPoint, point);
//				} while (lastPoint != point);
//				delete drawer;
//				pathing->FreePath(pathId);
//			}
////			circuit->GetGame()->SetPause(true, "Nub");
//		}, unit), FRAMES_PER_SEC);
//	}

	return 0; //signaling: OK
}

void CMilitaryManager::TestOrder()
{
	circuit->LOG("Hit 120th frame");
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
//		utils::free_clear(friendlies);
//	}
//	utils::free_clear(units);
}

} // namespace circuit
