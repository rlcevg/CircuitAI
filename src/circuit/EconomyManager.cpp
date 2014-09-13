/*
 * EconomyManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "EconomyManager.h"
#include "CircuitAI.h"
#include "GameAttribute.h"
#include "Scheduler.h"
#include "MetalManager.h"
#include "CircuitUnit.h"
#include "BuilderTask.h"
#include "FactoryTask.h"
#include "utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Map.h"
#include "SkirmishAIs.h"
#include "Resource.h"

namespace circuit {

using namespace springai;

CEconomyManager::CEconomyManager(CCircuitAI* circuit) :
		IModule(circuit),
		totalBuildpower(.0f)
{
	metalRes = circuit->GetCallback()->GetResourceByName("Metal");
	energyRes = circuit->GetCallback()->GetResourceByName("Energy");

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
	finishedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		Unit* u = unit->GetUnit();
		UnitDef* def = u->GetDef();
		this->totalBuildpower += def->GetBuildSpeed();

		AIFloat3 pos = u->GetPos();
		switch (u->GetBuildingFacing()) {
			case UNIT_FACING_SOUTH:
			default:
				pos.z += def->GetZSize() * SQUARE_SIZE;
				break;
			case UNIT_FACING_EAST:
				pos.x += def->GetXSize() * SQUARE_SIZE;
				break;
			case UNIT_FACING_NORTH:
				pos.z -= def->GetZSize() * SQUARE_SIZE;
				break;
			case UNIT_FACING_WEST:
				pos.x -= def->GetXSize() * SQUARE_SIZE;
				break;
		}
		u->MoveTo(pos, 0);

		PrepareFactory(unit);
		ExecuteFactory(unit);
	};
	idleHandler[unitDefId] = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			PrepareFactory(unit);
		}
		ExecuteFactory(unit);
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
		unit->RemoveTask();
	};

	/*
	 * comm handlers
	 */
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
//	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_0")->GetUnitDefId();
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_1")->GetUnitDefId();
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_2")->GetUnitDefId();
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_3")->GetUnitDefId();
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_4")->GetUnitDefId();
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;
	unitDefId = attrib->GetUnitDefByName("comm_trainer_support_5")->GetUnitDefId();
	finishedHandler[unitDefId] = commFinishedHandler;
	destroyedHandler[unitDefId] = commDestroyedHandler;

	/*
	 * armnanotc handlers
	 */
	unitDefId = attrib->GetUnitDefByName("armnanotc")->GetUnitDefId();
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
	finishedHandler[unitDefId] = [this](CCircuitUnit* unit) {
//		CCircuitAI* circuit = this->circuit;
//		CMetalManager& mm = circuit->GetGameAttribute()->GetMetalManager();
//		const CMetalManager::Metals& spots = mm.GetSpots();
//		const CMetalManager::Metal& spot = spots[rand() % spots.size()];
//		Unit* u = unit->GetUnit();
//
//		std::vector<float> params;
//		params.push_back(0.0f);
//		u->ExecuteCustomCommand(CMD_PRIORITY, params, 0, 100);
//
//		UnitDef* facDef = circuit->GetGameAttribute()->GetUnitDefByName("factorycloak");
//		AIFloat3 buildPos = circuit->FindBuildSiteMindMex(facDef, spot.position, 1000.0f, UNIT_COMMAND_BUILD_NO_FACING);
//		u->Build(facDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
		this->workers.insert(unit);
		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();

		PrepareBuilder(unit);
		ExecuteBuilder(unit);
	};
	idleHandler[unitDefId] = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			PrepareBuilder(unit);
		}
		ExecuteBuilder(unit);
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		this->workers.erase(unit);
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
		unit->RemoveTask();
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
	for (auto task : factoryTasks) {
		delete task;
	}
	for (auto task : builderTasks) {
		delete task;
	}
}

int CEconomyManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	printf("%s | %s | %s\n", unit->GetDef()->GetHumanName(), unit->GetDef()->GetName(), unit->GetDef()->GetWreckName());

	// InformUnfinished
	if (unit->GetUnit()->IsBeingBuilt() && builder != nullptr) {
		IConstructTask* task = static_cast<IConstructTask*>(builder->GetTask());
		unfinishedUnits[unit] = task;
		if (task != nullptr) {
			unfinishedTasks[task].push_back(unit);
			task->Progress();
		}
	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitFinished(CCircuitUnit* unit)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		IConstructTask* task = iter->second;
		if (task != nullptr) {
			std::list<CCircuitUnit*>& units = unfinishedTasks[task];
			if (task->IsDone()) {
				task->MarkCompleted();  // task will remove itself from owner on MarkCompleted
				for (auto u : units) {
					unfinishedUnits[u] = nullptr;
				}
				unfinishedTasks.erase(task);
				delete task;
			} else {
				units.remove(unit);
			}
		}
		unfinishedUnits.erase(iter);
	}

	auto search = finishedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != idleHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	if (unit->GetUnit()->IsBeingBuilt()) {
		auto iter = unfinishedUnits.find(unit);
		if (iter != unfinishedUnits.end()) {
			IConstructTask* task = iter->second;
			if (task != nullptr) {
				std::list<CCircuitUnit*>& units = unfinishedTasks[task];
				units.remove(iter->first);
				if (units.empty()) {
					unfinishedTasks.erase(task);
				}
				task->Regress();
			}
			unfinishedUnits.erase(iter);
		}
	}

	auto search = destroyedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != destroyedHandler.end()) {
		search->second(unit);
	}

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
	CBuilderTask* task = nullptr;
	for (auto t : builderTasks) {
		if (static_cast<CBuilderTask*>(t)->GetType() == CBuilderTask::TaskType::BUILD) {
			task = static_cast<CBuilderTask*>(t);
			break;
		}
	}
	if (task == nullptr) {
//		Map* map = circuit->GetMap();
//		int terWidth = map->GetWidth() * SQUARE_SIZE;
//		int terHeight = map->GetHeight() * SQUARE_SIZE;
//		float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
//		float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
//		AIFloat3 buildPos(x, map->GetElevationAt(x, z), z);
		if (workers.begin() != workers.end()) {
			AIFloat3 buildPos = (*workers.begin())->GetUnit()->GetPos();
			task = new CBuilderTask(CBuilderTask::Priority::LOW, 3, buildPos, builderTasks, CBuilderTask::TaskType::BUILD, 60);
		}
	}
}

CCircuitUnit* CEconomyManager::FindUnitToAssist(CCircuitUnit* unit)
{
	// TODO: Replace bruteforce with boost::rtree(linear/quadratic) query
	float temp = std::numeric_limits<float>::max();
	CCircuitUnit* target = unit;
	AIFloat3 pos = unit->GetUnit()->GetPos();
	for (auto& u : unfinishedUnits) {
		float dist = u.first->GetUnit()->GetPos().distance2D(pos);
		if (dist < temp) {
			temp = dist;
			target = u.first;
		}
	}
	return target;
}

void CEconomyManager::PrepareFactory(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	UnitDef* def = unit->GetDef();

	CFactoryTask* task = nullptr;
	decltype(factoryTasks)::iterator iter = factoryTasks.begin();
	for (; iter != factoryTasks.end(); ++iter) {
		if ((*iter)->CanAssignTo(unit)) {
			task = static_cast<CFactoryTask*>(*iter);
			break;
		}
	}

	if (task == nullptr) {
		AIFloat3 buildPos = u->GetPos();
		float radius = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE * 4;
		task = new CFactoryTask(CFactoryTask::Priority::LOW, 3, buildPos, factoryTasks, CFactoryTask::TaskType::DEFAULT, radius);
		iter = factoryTasks.begin();
	}

	task->AssignTo(unit);
//	if (task->IsFull()) {
		factoryTasks.splice(factoryTasks.end(), factoryTasks, iter);  // move task to back
//	}
}

void CEconomyManager::ExecuteFactory(CCircuitUnit* unit)
{
	CFactoryTask* task = static_cast<CFactoryTask*>(unit->GetTask());
	Unit* u = unit->GetUnit();
	AIFloat3 buildPos = u->GetPos();

	switch (task->GetType()) {
		default:
		case CFactoryTask::TaskType::BUILDPOWER: {
			UnitDef* buildDef = circuit->GetGameAttribute()->GetUnitDefByName("armrectr");
			u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING);
			break;
		}
		case CFactoryTask::TaskType::FIREPOWER: {

			break;
		}
	}
}

void CEconomyManager::PrepareBuilder(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	UnitDef* def = unit->GetDef();

	CBuilderTask* task = nullptr;
	decltype(builderTasks)::iterator iter = builderTasks.begin();
	for (; iter != builderTasks.end(); ++iter) {
		if ((*iter)->CanAssignTo(unit)) {
			task = static_cast<CBuilderTask*>(*iter);
			break;
		}
	}

	if (task == nullptr) {
		AIFloat3 pos = u->GetPos();
		task = new CBuilderTask(CBuilderTask::Priority::LOW, 3, pos, builderTasks, CBuilderTask::TaskType::ASSIST, 60 * 60);
		iter = builderTasks.begin();
	}

	task->AssignTo(unit);
//	if (task->IsFull()) {
		builderTasks.splice(builderTasks.end(), builderTasks, iter);  // move task to back
//	}
}

void CEconomyManager::ExecuteBuilder(CCircuitUnit* unit)
{
	CBuilderTask* task = static_cast<CBuilderTask*>(unit->GetTask());
	Unit* u = unit->GetUnit();
	AIFloat3 buildPos = u->GetPos();

	auto findBuildSite = [this](UnitDef* buildDef, AIFloat3& position) {
		Map* map = circuit->GetMap();
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
		return circuit->FindBuildSiteMindMex(buildDef, position, 1000.0f, facing);
	};

	switch (task->GetType()) {
		case CBuilderTask::TaskType::BUILD: {
			UnitDef* buildDef = circuit->GetGameAttribute()->GetUnitDefByName("armsolar");
			AIFloat3 position = u->GetPos();
			AIFloat3 buildPos;
			buildPos = findBuildSite(buildDef, position);
			if (buildPos == -RgtVector) {
				// TODO: Replace FindNearestSpots with FindNearestClusters
				CMetalManager::Metals spots = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpots(position, 5);
				for (auto& spot : spots) {
					buildPos = findBuildSite(buildDef, spot.position);
					if (buildPos != -RgtVector) {
						break;
					}
				}
			}
			if (buildPos == -RgtVector) {
				// Fallback to Guard/Assist
				CCircuitUnit* target = FindUnitToAssist(unit);
				u->Repair(target->GetUnit(), UNIT_COMMAND_OPTION_SHIFT_KEY);
			} else {
				u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
			}
			break;
		}
		default:
		case CBuilderTask::TaskType::ASSIST: {
			CCircuitUnit* target = FindUnitToAssist(unit);
			u->Repair(target->GetUnit(), UNIT_COMMAND_OPTION_SHIFT_KEY);
			break;
		}
	}
}

} // namespace circuit
