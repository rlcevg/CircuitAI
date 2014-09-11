/*
 * EconomyManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "EconomyManager.h"
#include "EconomyTask.h"
#include "CircuitAI.h"
#include "GameAttribute.h"
#include "Scheduler.h"
#include "MetalManager.h"
#include "CircuitUnit.h"
#include "utils.h"

#include "OOAICallback.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Map.h"
#include "SkirmishAIs.h"

//#include "Command.h"
#include "AISCommands.h"

namespace circuit {

using namespace springai;

CEconomyManager::CEconomyManager(CCircuitAI* circuit) :
		IModule(circuit),
		totalBuildpower(.0f)
{
	// TODO: Use A* ai planning... or sth... STRIPS https://ru.wikipedia.org/wiki/STRIPS
	//       https://ru.wikipedia.org/wiki/Марковский_процесс_принятия_решений
	CGameAttribute* attrib = circuit->GetGameAttribute();
	int unitDefId;

	SkirmishAIs* ais = circuit->GetCallback()->GetSkirmishAIs();
	int aisCount = ais->GetSize();
	delete ais;
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::Update, this), aisCount, circuit->GetSkirmishAIId());

	// TODO: Group handlers
	//       Raider:       Glaive, Bandit, Scorcher, Pyro, Panther, Scrubber, Duck
	//       Assault:      Zeus, Thug, Ravager, Hermit, Reaper
	//       Skirmisher:   Rocko, Rogue, Recluse, Scalpel, Buoy
	//       Riot:         Warrior, Outlaw, Leveler, Mace, Scallop
	//       Artillery:    Hammer, Wolverine, Impaler, Firewalker, Pillager, Tremor
	//       Scout:        Flea, Dart, Puppy
	//       Anti-Air:     Gremlin, Vandal, Crasher, Archangel, Tarantula, Copperhead, Flail, Angler
	//       Support:      Slasher, Penetrator, Felon, Moderator, (Dominatrix?)
	//       Mobile Bombs: Tick, Roach, Skuttle
	//       Shield
	//       Cloaker

	/*
	 * factorycloak handlers
	 */
	unitDefId = attrib->GetUnitDefByName("factorycloak")->GetUnitDefId();
	createdHandler[unitDefId] = [this](CCircuitUnit* unit) {
	};
	finishedHandler[unitDefId] = [this](CCircuitUnit* unit) {
//		CCircuitAI* circuit = this->circuit;
//		Unit* u = unit->GetUnit();
//		CGameAttribute* gameAttribute = circuit->GetGameAttribute();
//		UnitDef* def1 = gameAttribute->GetUnitDefByName("armpw");
//		UnitDef* def2 = gameAttribute->GetUnitDefByName("armrectr");
//		UnitDef* def3 = gameAttribute->GetUnitDefByName("armrock");
//		u->SetRepeat(true, 0);
//		AIFloat3 buildPos(0, 0, 0);
//		u->Build(def1, buildPos, -1, 0);
//		u->Build(def2, buildPos, -1, 0);
//		u->Build(def3, buildPos, -1, UNIT_COMMAND_OPTION_ALT_KEY);
//		u->Build(def3, buildPos, -1, 0);
//
//		AIFloat3 pos = u->GetPos();
//		switch (u->GetBuildingFacing()) {
//			case UNIT_FACING_SOUTH:
//			default:
//				pos.z += unit->GetDef()->GetZSize() * SQUARE_SIZE;
//				break;
//			case UNIT_FACING_EAST:
//				pos.x += unit->GetDef()->GetXSize() * SQUARE_SIZE;
//				break;
//			case UNIT_FACING_NORTH:
//				pos.z -= unit->GetDef()->GetZSize() * SQUARE_SIZE;
//				break;
//			case UNIT_FACING_WEST:
//				pos.x -= unit->GetDef()->GetXSize() * SQUARE_SIZE;
//				break;
//		}
//
//		u->MoveTo(pos, 0);

		bool assigned = false;
		for (auto& task : this->tasks) {
			assigned = task->Execute(unit);
			if (assigned) {
				break;
			}
		}

		if (!assigned) {
//			CEconomyTask* task = new CEconomyTask(CEconomyTask::TaskType::DEFAULT);
//			tasks.push_back(task);
//			lazyUnits.push_back(unit);
		}

		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
	};

	/*
	 * comm handlers
	 */
	auto commCreatedHandler = [this](CCircuitUnit* unit) {
	};
	auto commFinishedHandler = [this](CCircuitUnit* unit) {
		CCircuitAI* circuit = this->circuit;
		Unit* u = unit->GetUnit();
		Map* map = circuit->GetMap();
		UnitDef* facDef = circuit->GetGameAttribute()->GetUnitDefByName("factorycloak");
//		UnitDef* mexDef = circuit->GetGameAttribute()->GetUnitDefByName("cormex");
		AIFloat3 position = u->GetPos();

		int facing = 0;
		float terWidth = map->GetWidth() * SQUARE_SIZE;
		float terHeight = map->GetHeight() * SQUARE_SIZE;
		if (math::fabs(terWidth - 2 * position.x) > math::fabs(terHeight - 2 * position.z)) {
			if (2 * position.x > terWidth) {
				facing = UNIT_FACING_WEST;
			} else {
				facing = UNIT_FACING_EAST;
			}
		} else {
			if (2 * position.z > terHeight) {
				facing = UNIT_FACING_NORTH;
			} else {
				facing = UNIT_FACING_SOUTH;
			}
		}

		AIFloat3 buildPos = this->circuit->FindBuildSiteMindMex(facDef, position, 1000.0f, facing);
		u->Build(facDef, buildPos, facing, 0);

		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();
	};
	auto commDestroyedHandler = [this](CCircuitUnit* unit) {
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
	};

	unitDefId = attrib->GetUnitDefByName("armcom1")->GetUnitDefId();
	createdHandler[unitDefId] = commCreatedHandler;
//	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_0")->GetUnitDefId();
	createdHandler[unitDefId] = commCreatedHandler;
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_1")->GetUnitDefId();
	createdHandler[unitDefId] = commCreatedHandler;
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_2")->GetUnitDefId();
	createdHandler[unitDefId] = commCreatedHandler;
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_3")->GetUnitDefId();
	createdHandler[unitDefId] = commCreatedHandler;
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_4")->GetUnitDefId();
	createdHandler[unitDefId] = commCreatedHandler;
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_5")->GetUnitDefId();
	createdHandler[unitDefId] = commCreatedHandler;
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;

	/*
	 * armnanotc handlers
	 */
	unitDefId = attrib->GetUnitDefByName("armnanotc")->GetUnitDefId();
	createdHandler[unitDefId] = [this](CCircuitUnit* unit) {
	};
	finishedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		CCircuitAI* circuit = this->circuit;
		Unit* u =unit->GetUnit();
		UnitDef* def = unit->GetDef();
		AIFloat3 toPos = u->GetPos();
		float size = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE;
		toPos.x += (toPos.x > circuit->GetMap()->GetWidth() * SQUARE_SIZE / 2) ? -size : size;
		toPos.z += (toPos.z > circuit->GetMap()->GetHeight() * SQUARE_SIZE / 2) ? -size : size;
		u->SetRepeat(true, 0);  // not necessary, maybe for later use
		// Use u->Fight(toPos, 0) when AI is in same team as human or disable "Auto Patrol Nanos" widget
		u->PatrolTo(toPos, 0);

		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
	};

	/*
	 * armrectr handlers
	 */
	unitDefId = attrib->GetUnitDefByName("armrectr")->GetUnitDefId();
	createdHandler[unitDefId] = [this](CCircuitUnit* unit) {
	};
	finishedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		CCircuitAI* circuit = this->circuit;
		CMetalManager& mm = circuit->GetGameAttribute()->GetMetalManager();
		const CMetalManager::Metals& spots = mm.GetSpots();
		const CMetalManager::Metal& spot = spots[rand() % spots.size()];
		Unit* u = unit->GetUnit();

		std::vector<float> params;
		params.push_back(0.0f);
		u->ExecuteCustomCommand(CMD_PRIORITY, params, 0, 100);

		UnitDef* facDef = circuit->GetGameAttribute()->GetUnitDefByName("factorycloak");
		AIFloat3 buildPos = circuit->FindBuildSiteMindMex(facDef, spot.position, 1000.0, -1);
		u->Build(facDef, buildPos, -1, UNIT_COMMAND_OPTION_SHIFT_KEY);
		this->workers.insert(unit);

		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
		workers.erase(unit);
	};

//	CGameAttribute::UnitDefs& defs = circuit->GetGameAttribute()->GetUnitDefs();
//	for (auto& pair : defs) {
//		UnitDef* def = pair.second;
//		if (def->IsBuilder()) {
//			if (!def->GetBuildOptions().empty()) {
//				finishedHandler[def->GetUnitDefId()] = workerFinishedHandler;
//			} else {
//				finishedHandler[def->GetUnitDefId()] = nanoFinishedHandler;
//			}
//		}
//	}
}

CEconomyManager::~CEconomyManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (auto task : tasks) {
		delete task;
	}
}

int CEconomyManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	Unit* u = unit->GetUnit();
	UnitDef* def = u->GetDef();
	printf("%s | %s | %s\n", def->GetHumanName(), def->GetName(), def->GetWreckName());
	delete def;

	auto search = createdHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != createdHandler.end()) {
		search->second(unit);
	}

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

//	else {
//		if (def->IsBuilder()) {
//			if (!def->GetBuildOptions().empty()) {
//				workers.insert(unit);
//			} else {
//				// More Nanos? Make them work
//				Unit* u =unit->GetUnit();
//				AIFloat3 toPos = u->GetPos();
//				float size = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE;
//				toPos.x += (toPos.x > circuit->GetMap()->GetWidth() * SQUARE_SIZE / 2) ? -size : size;
//				toPos.z += (toPos.z > circuit->GetMap()->GetHeight() * SQUARE_SIZE / 2) ? -size : size;
//				u->SetRepeat(true, 0); // not necessary, maybe for later use
//				// Use u->Fight(toPos, 0) when AI is in same team as human or disable "Auto Patrol Nanos" widget
//				u->PatrolTo(toPos, 0);
//			}
//			totalBuildpower += def->GetBuildSpeed();
//		}
//	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	UnitDef* def = unit->GetDef();
	auto search = destroyedHandler.find(def->GetUnitDefId());
	if (search != destroyedHandler.end()) {
		search->second(unit);
	}

//	else {
//		auto search = workers.find(unit);
//		if (search != workers.end()) {
//			totalBuildpower -= def->GetBuildSpeed();
//			workers.erase(search);
//		}
//	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId)
{
	UnitCreated(unit, nullptr);
	UnitFinished(unit);
	return 0; //signaling: OK
}

int CEconomyManager::UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId)
{
	UnitDestroyed(unit, nullptr);
	return 0; //signaling: OK
}

void CEconomyManager::Update()
{

}

} // namespace circuit
