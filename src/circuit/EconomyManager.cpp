/*
 * EconomyManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "EconomyManager.h"
#include "CircuitAI.h"
#include "GameAttribute.h"
#include "MetalManager.h"
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
		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();
	};
	finishedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		CCircuitAI* circuit = this->circuit;
		Unit* u = unit->GetUnit();
		CGameAttribute* gameAttribute = circuit->GetGameAttribute();
		UnitDef* def1 = gameAttribute->GetUnitDefByName("armpw");
		UnitDef* def2 = gameAttribute->GetUnitDefByName("armrectr");
		UnitDef* def3 = gameAttribute->GetUnitDefByName("armrock");
		u->SetRepeat(true, 0);
		AIFloat3 buildPos(0, 0, 0);
		u->Build(def1, buildPos, 0, 0);
		u->Build(def2, buildPos, 0, 0);
		u->Build(def3, buildPos, 0, 0);
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
	};

	/*
	 * comm handlers
	 */
	auto commCreatedHandler = [this](CCircuitUnit* unit) {
		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();
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
				facing = 3;  // facing="west"
			} else {
				facing = 1;  // facing="east"
			}
		} else {
			if (2 * position.z > terHeight) {
				facing = 2;  // facing="north"
			} else {
				facing = 0;  // facing="south"
			}
		}

//		AIFloat3 buildPos(position);
//		float xsize = facDef->GetXSize() * SQUARE_SIZE;
//		float zsize = facDef->GetZSize() * SQUARE_SIZE;
//		float xsize2 = mexDef->GetXSize() * SQUARE_SIZE / 2;
//		float zsize2 = mexDef->GetZSize() * SQUARE_SIZE / 2;
//		float searchDiameter = std::min(xsize, zsize);
//		printf("Xsize: %f, Zsize: %f\n", xsize, zsize);
//		AIFloat3 offset(xsize + xsize2, 0, zsize + zsize2);
//		std::array<AIFloat3, 4> offsets = {AIFloat3(offset.x, 0, offset.z), AIFloat3(-offset.x, 0, offset.z), AIFloat3(-offset.x, 0, -offset.z), AIFloat3(offset.x, 0, -offset.z)};
//		for (auto& offset : offsets) {
//			AIFloat3 probe = position + offset;
//			AIFloat3 res = map->FindClosestBuildSite(facDef, probe, searchDiameter / 2, 4, facing);
//			if (res != AIFloat3(-1, 0, 0)) {
//				// if () {} Check box mex overlap
//				buildPos = res;
//				break;
//			}
//		}
//		// if buildPos == position then terraform

		AIFloat3 buildPos = this->circuit->FindBuildSiteMindMex(facDef, position, 1000.0f, facing);
		u->Build(facDef, buildPos, facing, 0);
	};
	auto commDestroyedHandler = [this](CCircuitUnit* unit) {
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
	};

	unitDefId = attrib->GetUnitDefByName("armcom1")->GetUnitDefId();
	createdHandler[unitDefId] = commCreatedHandler;
	finishedHandler[unitDefId] = commFinishedHandler;
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
		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();
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
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
	};

	/*
	 * armrectr handlers
	 */
	unitDefId = attrib->GetUnitDefByName("armrectr")->GetUnitDefId();
	createdHandler[unitDefId] = [this](CCircuitUnit* unit) {
		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();
	};
	finishedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		CCircuitAI* circuit = this->circuit;
		CMetalManager& mm = circuit->GetGameAttribute()->GetMetalManager();
		const CMetalManager::Metals& spots = mm.GetSpots();
		const CMetalManager::Metal& spot = spots[rand() % spots.size()];
		Unit* u = unit->GetUnit();
		UnitDef* facDef = circuit->GetGameAttribute()->GetUnitDefByName("factorycloak");
		AIFloat3 buildPos = circuit->FindBuildSiteMindMex(facDef, spot.position, 1000.0, -1);
		u->Build(facDef, buildPos, -1, 0);
		this->workers.insert(unit);
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

} // namespace circuit
