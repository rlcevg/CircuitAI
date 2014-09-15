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
//#include "BuilderTask.h"
#include "FactoryTask.h"
#include "utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Map.h"
#include "SkirmishAIs.h"
#include "Resource.h"
#include "Economy.h"

// debug
//#include "WrappTeam.h"
//#include "TeamRulesParam.h"
//#include "Drawer.h"

namespace circuit {

using namespace springai;

CEconomyManager::CEconomyManager(CCircuitAI* circuit) :
		IModule(circuit),
		totalBuildpower(.0f),
		builderPower(.0f),
		factoryPower(.0f),
		builderTasksSize(0),
		cachedFrame(-1),
		isCachedChanged(true)
{
	metalRes = circuit->GetCallback()->GetResourceByName("Metal");
	energyRes = circuit->GetCallback()->GetResourceByName("Energy");
	eco = circuit->GetCallback()->GetEconomy();
	std::vector<WorkerInfo*> wi;
	wtRelation.push_back(wi);

	// TODO: Use A* ai planning... or sth... STRIPS https://ru.wikipedia.org/wiki/STRIPS
	//       https://ru.wikipedia.org/wiki/Марковский_процесс_принятия_решений
	CGameAttribute* attrib = circuit->GetGameAttribute();
	int unitDefId;

	SkirmishAIs* ais = circuit->GetCallback()->GetSkirmishAIs();
	int aisCount = ais->GetSize();
	delete ais;
	// FIXME: Remove parallel clusterization (and Init). Task is fast enough for main process and too much issues with parallelism.
	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>(&CEconomyManager::Init, this), 0);
	const int numUpdates = 2;
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateFactoryPower, this), aisCount * numUpdates, circuit->GetSkirmishAIId() + 0);
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateBuilderPower, this), aisCount * numUpdates, circuit->GetSkirmishAIId() + 1);
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::WorkerWatchdog, this), FRAMES_PER_SEC * 22, circuit->GetSkirmishAIId());

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
		AIFloat3 pos = u->GetPos();
		float buildSpeed = def->GetBuildSpeed();
		this->totalBuildpower += buildSpeed;
		this->factoryPower += buildSpeed;

		// check nanos around
		std::list<CCircuitUnit*> nanos;
		UnitDef* nanoDef = this->circuit->GetGameAttribute()->GetUnitDefByName("armnanotc");
		float radius = nanoDef->GetBuildDistance();
		std::vector<Unit*> units = this->circuit->GetCallback()->GetFriendlyUnitsIn(u->GetPos(), radius);
		int nanoId = nanoDef->GetUnitDefId();
		int teamId = this->circuit->GetTeamId();
		for (auto nano : units) {
			UnitDef* ndef = nano->GetDef();
			if (ndef->GetUnitDefId() == nanoId && nano->GetTeam() == teamId) {
				nanos.push_back(this->circuit->GetUnitById(nano->GetUnitId()));
			}
			delete ndef;
		}
		utils::free_clear(units);
		factories[unit] = nanos;

		// check factory's cluster
		int index = this->circuit->GetGameAttribute()->GetMetalManager().FindNearestCluster(pos);
		if (index >= 0) {
			clusterInfo[index].factory = unit;
		}

//		switch (u->GetBuildingFacing()) {
//			case UNIT_FACING_SOUTH:
//			default:
//				pos.z += def->GetZSize() * 0.75 * SQUARE_SIZE;
//				break;
//			case UNIT_FACING_EAST:
//				pos.x += def->GetXSize() * 0.75 * SQUARE_SIZE;
//				break;
//			case UNIT_FACING_NORTH:
//				pos.z -= def->GetZSize() * 0.75 * SQUARE_SIZE;
//				break;
//			case UNIT_FACING_WEST:
//				pos.x -= def->GetXSize() * 0.75 * SQUARE_SIZE;
//				break;
//		}
		float size = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE * 0.75;
		pos.x += (pos.x > this->circuit->GetMap()->GetWidth() * SQUARE_SIZE / 2) ? -size : size;
		pos.z += (pos.z > this->circuit->GetMap()->GetHeight() * SQUARE_SIZE / 2) ? -size : size;

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
		float buildSpeed = unit->GetDef()->GetBuildSpeed();
		this->totalBuildpower -= buildSpeed;
		this->factoryPower -= buildSpeed;
		factories.erase(unit);
		unit->RemoveTask();
		for (auto& info : clusterInfo) {
			info.factory = nullptr;
		}
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

		int facing = UNIT_COMMAND_BUILD_NO_FACING;
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
		Unit* u = unit->GetUnit();
		UnitDef* def = unit->GetDef();
		AIFloat3 fromPos = u->GetPos();
		AIFloat3 toPos = fromPos;
		float size = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE;
		toPos.x += (toPos.x > circuit->GetMap()->GetWidth() * SQUARE_SIZE / 2) ? -size : size;
		toPos.z += (toPos.z > circuit->GetMap()->GetHeight() * SQUARE_SIZE / 2) ? -size : size;
		u->SetRepeat(true, 0);  // not necessary, maybe for later use
		// Use u->Fight(toPos, 0) when AI is in same team as human or disable "Auto Patrol Nanos" widget
		u->PatrolTo(toPos, 0);

		std::vector<float> params;
		params.push_back(0.0f);
		u->ExecuteCustomCommand(CMD_PRIORITY, params);

		// check to which factory nano belongs to
		float buildSpeed = def->GetBuildSpeed();
		this->totalBuildpower += buildSpeed;
		this->factoryPower += buildSpeed;
		float radius = def->GetBuildDistance();
		float qradius = radius * radius;
		auto qdist = [](AIFloat3& pos1, AIFloat3& pos2) {
			float dx = pos1.x - pos2.x;
			float dz = pos1.z - pos2.z;
			return dx * dx + dz * dz;
		};
		for (auto& fac : factories) {
			AIFloat3 facPos = fac.first->GetUnit()->GetPos();
			if (qdist(facPos, fromPos) < qradius) {
				fac.second.push_back(unit);
			}
		}
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		float buildSpeed = unit->GetDef()->GetBuildSpeed();
		this->totalBuildpower -= buildSpeed;
		this->factoryPower -= buildSpeed;
		for (auto& fac : factories) {
			fac.second.remove(unit);
		}
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
		float buildSpeed = unit->GetDef()->GetBuildSpeed();
		this->totalBuildpower += buildSpeed;
		this->builderPower += buildSpeed;
		this->workers.insert(unit);
		isCachedChanged = true;

//		PrepareBuilder(unit);
//		ExecuteBuilder(unit);
	};
	idleHandler[unitDefId] = [this](CCircuitUnit* unit) {
		unit->RemoveTask();
		PrepareBuilder(unit);
		ExecuteBuilder(unit);
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		float buildSpeed = unit->GetDef()->GetBuildSpeed();
		this->totalBuildpower -= buildSpeed;
		this->builderPower -= buildSpeed;
		this->workers.erase(unit);
		isCachedChanged = true;
		this->builderInfo.erase(unit);
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
	utils::free_clear(factoryTasks);
	for (auto& tasks : builderTasks) {
		utils::free_clear(tasks.second);
	}
	utils::free_clear(wtRelation.front());
	delete metalRes, energyRes, eco;
}

int CEconomyManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	printf("%s | %s | %s\n", unit->GetDef()->GetHumanName(), unit->GetDef()->GetName(), unit->GetDef()->GetWreckName());

	// InformUnfinished
	if (unit->GetUnit()->IsBeingBuilt() && builder != nullptr) {
		IConstructTask* task = static_cast<IConstructTask*>(builder->GetTask());
		if (task != nullptr) {
			switch (task->GetConstructType()) {
				case IConstructTask::ConstructType::FACTORY: {
//					static_cast<CFactoryTask*>(task)->Progress();
					break;
				}
				case IConstructTask::ConstructType::BUILDER: {
					static_cast<CBuilderTask*>(task)->SetTarget(unit);
					break;
				}
			}
			unfinishedUnits[unit] = task;
			unfinishedTasks[task].push_back(unit);
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
			switch (task->GetConstructType()) {
				case IConstructTask::ConstructType::FACTORY: {
					CFactoryTask* taskF = static_cast<CFactoryTask*>(task);
					taskF->Progress();
					std::list<CCircuitUnit*>& units = unfinishedTasks[task];
					if (taskF->IsDone()) {
						taskF->MarkCompleted();
						factoryTasks.remove(taskF);
						isCachedChanged = true;
						for (auto u : units) {
							unfinishedUnits[u] = nullptr;
						}
						unfinishedTasks.erase(task);
						delete taskF;
					} else {
						units.remove(unit);
					}
					break;
				}
				case IConstructTask::ConstructType::BUILDER: {
					CBuilderTask* taskB = static_cast<CBuilderTask*>(task);
					taskB->MarkCompleted();
					builderTasks[taskB->GetType()].remove(taskB);
					isCachedChanged = true;
					delete taskB;
					builderTasksSize--;
					break;
				}
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
			IConstructTask* task = static_cast<IConstructTask*>(iter->second);
			if (task != nullptr) {
				switch (task->GetConstructType()) {
					case IConstructTask::ConstructType::FACTORY: {
						std::list<CCircuitUnit*>& units = unfinishedTasks[task];
						units.remove(iter->first);
						if (units.empty()) {
							unfinishedTasks.erase(task);
						}
						static_cast<CFactoryTask*>(task)->Regress();
						break;
					}
					case IConstructTask::ConstructType::BUILDER: {
						unfinishedTasks.erase(task);
						static_cast<CBuilderTask*>(task)->SetTarget(nullptr);
						break;
					}
				}
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

void CEconomyManager::Init()
{
	const std::vector<CMetalManager::MetalIndices>& clusters = circuit->GetGameAttribute()->GetMetalManager().GetClusters();
	clusterInfo.resize(clusters.size());
	for (int i = 0; i < clusters.size(); i++) {
		clusterInfo[i] = {nullptr};
	}
}

void CEconomyManager::UpdateFactoryPower()
{
//	printf("builderTasksSize: %i\n", builderTasksSize);
	if (builderTasksSize >= workers.size() * 2) {
		return;
	}

	float metalIncome = eco->GetIncome(metalRes);
//	printf("metalIncome: %.2f, totalBuildpower: %.2f, factoryPower: %.2f\n", metalIncome, totalBuildpower, factoryPower);
	if (factoryPower < metalIncome) {
//		printf("FAC: %i, NANO: %i \n", builderTasks[CBuilderTask::TaskType::FACTORY].size(), builderTasks[CBuilderTask::TaskType::NANO].size());
		if (builderTasks[CBuilderTask::TaskType::FACTORY].empty() && builderTasks[CBuilderTask::TaskType::NANO].empty()) {
			CCircuitUnit* factory = nullptr;
			for (auto& fac : factories) {
				if (fac.second.size() < 4) {
					factory = fac.first;
					break;
				}
			}
			if (factory != nullptr) {
				Unit* u = factory->GetUnit();
				UnitDef* def = factory->GetDef();
				AIFloat3 buildPos = u->GetPos();
				switch (u->GetBuildingFacing()) {
					case UNIT_FACING_SOUTH:
					default:
						buildPos.z -= def->GetZSize() * 0.75 * SQUARE_SIZE;
						break;
					case UNIT_FACING_EAST:
						buildPos.x -= def->GetXSize() * 0.75 * SQUARE_SIZE;
						break;
					case UNIT_FACING_NORTH:
						buildPos.z += def->GetZSize() * 0.75 * SQUARE_SIZE;
						break;
					case UNIT_FACING_WEST:
						buildPos.x += def->GetXSize() * 0.75 * SQUARE_SIZE;
						break;
				}
				CBuilderTask* task = new CBuilderTask(CBuilderTask::Priority::LOW, buildPos, CBuilderTask::TaskType::NANO, 1);
				builderTasks[CBuilderTask::TaskType::NANO].push_front(task);
				builderTasksSize++;
				isCachedChanged = true;
			} else {
				AIFloat3 startPos = circuit->GetStartPos();
				CMetalManager::MetalPredicate predicate = [this](const CMetalManager::MetalNode& v) {
					return clusterInfo[v.second].factory == nullptr;
				};
				int index = circuit->GetGameAttribute()->GetMetalManager().FindNearestCluster(startPos, predicate);
				AIFloat3 buildPos;
				if (index >= 0) {
					const std::vector<AIFloat3>& centroids = circuit->GetGameAttribute()->GetMetalManager().GetCentroids();
					buildPos = centroids[index];
				} else {
					Map* map = circuit->GetMap();
					int terWidth = map->GetWidth() * SQUARE_SIZE;
					int terHeight = map->GetHeight() * SQUARE_SIZE;
					float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
					float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
					buildPos = AIFloat3(x, map->GetElevationAt(x, z), z);
				}
				CBuilderTask* task = new CBuilderTask(CBuilderTask::Priority::LOW, buildPos, CBuilderTask::TaskType::FACTORY, 1);
				builderTasks[CBuilderTask::TaskType::FACTORY].push_front(task);
				builderTasksSize++;
				isCachedChanged = true;
			}
		}
	}
}

void CEconomyManager::UpdateBuilderPower()
{
	if (factoryTasks.size() >= factories.size() * 2) {
		return;
	}

	float metalIncome = eco->GetIncome(metalRes);
//	printf("metalIncome: %.2f, totalBuildpower: %.2f, factoryPower: %.2f\n", metalIncome, totalBuildpower, factoryPower);
	if (builderPower < metalIncome * 1.5 && !factories.empty()) {
		for (auto task : factoryTasks) {
			if (static_cast<CFactoryTask*>(task)->GetType() == CFactoryTask::TaskType::BUILDPOWER) {
				return;
			}
		}
		auto iter = factories.begin();
		std::advance(iter, rand() % factories.size());
		CCircuitUnit* factory = iter->first;
		AIFloat3 buildPos = factory->GetUnit()->GetPos();
		Map* map = circuit->GetMap();
		float radius = std::max(map->GetWidth(), map->GetHeight()) * SQUARE_SIZE / 4;
		CFactoryTask* task = new CFactoryTask(CFactoryTask::Priority::LOW, buildPos, CFactoryTask::TaskType::BUILDPOWER, 2, radius);
		factoryTasks.push_front(task);
	}
}

void CEconomyManager::WorkerWatchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	decltype(builderInfo)::iterator iter = builderInfo.begin();
	while (iter != builderInfo.end()) {
		CBuilderTask* task = static_cast<CBuilderTask*>(iter->first->GetTask());
		if (task != nullptr) {
			int duration = task->GetDuration();
			if ((duration > 0) && (circuit->GetLastFrame() - iter->second.startFrame < duration)) {
				switch (task->GetType()) {
					case CBuilderTask::TaskType::ASSIST: {
						iter->first->GetUnit()->Stop();
						iter = builderInfo.erase(iter);
						continue;
						break;
					}
				}
			}
		}
		++iter;
	}
}

CCircuitUnit* CEconomyManager::FindUnitToAssist(CCircuitUnit* unit)
{
	Unit* cu = unit->GetUnit();
	AIFloat3 pos = cu->GetPos();
	float radius = unit->GetDef()->GetBuildDistance() + cu->GetMaxSpeed() * FRAMES_PER_SEC * 5;
	std::vector<Unit*> units = circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius);
	for (auto u : units) {
		if (u->GetHealth() < u->GetMaxHealth() && u->GetSpeed() <= cu->GetMaxSpeed() * 2) {
			return circuit->GetUnitById(u->GetUnitId());
		}
	}
	utils::free_clear(units);
	return unit;
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
		task = new CFactoryTask(CFactoryTask::Priority::LOW, buildPos, CFactoryTask::TaskType::DEFAULT, 2, radius);
		factoryTasks.push_front(task);
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
			UnitDef* buildDef = circuit->GetGameAttribute()->GetUnitDefByName("armpw");
			u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING);
			break;
		}
	}
}

void CEconomyManager::PrepareBuilder(CCircuitUnit* unit)
{
	auto qdist = [](const AIFloat3& p1, const AIFloat3& p2) {
		float x = p1.x - p2.x;
		float z = p1.z - p2.z;
		return x * x + z * z;
	};

	std::vector<const CBuilderTask*> candidates;
	WorkerInfo* unitInfo;
	// TODO: Refactor task picker. Task with min(dist*quantity) should be enough
	WorkerTaskRelation& wtRelation = GetWorkerTaskRelations(unit, unitInfo);
	int idx = wtRelation.front().size();
	int i = 0;
	for (auto& tasks : builderTasks) {
		for (auto& task : tasks.second) {
			if (task->CanAssignTo(unit)) {
				const CBuilderTask* candidate = static_cast<CBuilderTask*>(task);
				auto iter = std::find(wtRelation[i].begin(), wtRelation[i].end(), unitInfo);
				int icand = std::distance(wtRelation[i].begin(), iter);
				if (icand < idx) {
					idx = icand;
					candidates.clear();
					candidates.push_back(candidate);
				} else if (icand == idx) {
					candidates.push_back(candidate);
				}
			}
			i++;
		}
	}

	CBuilderTask* task = nullptr;
	if (!candidates.empty()) {
		task = (CBuilderTask*)candidates.front();
		float dist = qdist(task->GetPos(), unitInfo->pos);
		int quantity = task->GetQuantity();
		for (auto t : candidates) {
			float d = qdist(t->GetPos(), unitInfo->pos);
			int q = ((CBuilderTask*)t)->GetQuantity();
			if ((q < quantity) || (q == quantity && d < dist)) {
//			if ((q < quantity) || (q * d < quantity * dist)) {
				quantity = q;
				dist = d;
				task = (CBuilderTask*)t;
			}
		}
	}

	if (task == nullptr) {
		AIFloat3 pos = unit->GetUnit()->GetPos();
//		task = new CBuilderTask(CBuilderTask::Priority::LOW, 3, pos, builderTasks, CBuilderTask::TaskType::ASSIST, 60, FRAMES_PER_SEC * 60);
		task = new CBuilderTask(CBuilderTask::Priority::LOW, pos, CBuilderTask::TaskType::ENERGIZE, 1);
		builderTasks[CBuilderTask::TaskType::ENERGIZE].push_front(task);
		builderTasksSize++;
		isCachedChanged = true;
	}

	task->AssignTo(unit);
//	builderTasks.pop_front();
//	builderTasks.push_back(task);
//
//	AIFloat3 pos1 = task->GetBuildPos();
//	AIFloat3 pos2 = task->GetPos();
//	printf("task: %i, %i | pos1: %.2f, %.2f | pos2: %.2f, %.2f\n", task->GetType(), task->GetTarget(), pos1.x, pos1.z, pos2.x, pos2.z);
//	circuit->GetDrawer()->DeletePointsAndLines(pos1);
//	circuit->GetDrawer()->DeletePointsAndLines(pos2);
//	circuit->GetDrawer()->AddPoint(pos1, "buildPos");
//	circuit->GetDrawer()->AddPoint(pos2, "position");
}

void CEconomyManager::ExecuteBuilder(CCircuitUnit* unit)
{
	CBuilderTask* task = static_cast<CBuilderTask*>(unit->GetTask());
	Unit* u = unit->GetUnit();

	auto findBuildSite = [this](UnitDef* buildDef, const AIFloat3& position, int& facing) {
		Map* map = circuit->GetMap();
		facing = UNIT_COMMAND_BUILD_NO_FACING;
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
		return circuit->FindBuildSiteMindMex(buildDef, position, 800.0f, facing);
	};
	auto assist = [this](CCircuitUnit* unit) {
		// FIXME: Create ASSIST task (only for this unit)
		CCircuitUnit* target = FindUnitToAssist(unit);
		unit->GetUnit()->Repair(target->GetUnit(), UNIT_COMMAND_OPTION_SHIFT_KEY);
		builderInfo[unit].startFrame = circuit->GetLastFrame();
	};

	switch (task->GetType()) {
		case CBuilderTask::TaskType::FACTORY: {
			std::vector<float> params;
			params.push_back(2.0f);
			u->ExecuteCustomCommand(CMD_PRIORITY, params);

			if (task->GetTarget() != nullptr) {
				u->Repair(task->GetTarget()->GetUnit(), UNIT_COMMAND_OPTION_SHIFT_KEY);
				break;
			}
			UnitDef* buildDef = circuit->GetGameAttribute()->GetUnitDefByName("factorycloak");
			AIFloat3 buildPos = task->GetBuildPos();
			if (buildPos != -RgtVector && circuit->GetMap()->IsPossibleToBuildAt(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING)) {
				u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
				break;
			}
			AIFloat3 position = task->GetPos();
			int facing;
			buildPos = findBuildSite(buildDef, position, facing);
			if (buildPos == -RgtVector) {
				// TODO: Replace FindNearestSpots with FindNearestClusters
				const CMetalManager::Metals& spots =  circuit->GetGameAttribute()->GetMetalManager().GetSpots();
				CMetalManager::MetalIndices indices = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpots(position, 3);
				for (const int idx : indices) {
					buildPos = findBuildSite(buildDef, spots[idx].position, facing);
					if (buildPos != -RgtVector) {
						break;
					}
				}
			}
			if (buildPos != -RgtVector) {
				task->SetBuildPos(buildPos);
				u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_SHIFT_KEY);
			} else {
				task->MarkCompleted();
				builderTasks[CBuilderTask::TaskType::FACTORY].remove(task);
				isCachedChanged = true;
				delete task;
				builderTasksSize--;
				// Fallback to Guard/Assist
				assist(unit);
			}
			break;
		}
		case CBuilderTask::TaskType::NANO: {
			std::vector<float> params;
			params.push_back(0.0f);
			u->ExecuteCustomCommand(CMD_PRIORITY, params);

			if (task->GetTarget() != nullptr) {
				u->Repair(task->GetTarget()->GetUnit(), UNIT_COMMAND_OPTION_SHIFT_KEY);
				break;
			}
			UnitDef* buildDef = circuit->GetGameAttribute()->GetUnitDefByName("armnanotc");
			AIFloat3 buildPos = task->GetBuildPos();
			if (buildPos != -RgtVector && circuit->GetMap()->IsPossibleToBuildAt(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING)) {
				u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
				break;
			}
			AIFloat3 position = task->GetPos();
			int facing;
			buildPos = findBuildSite(buildDef, position, facing);
			if (buildPos == -RgtVector) {
				// TODO: Replace FindNearestSpots with FindNearestClusters
				const CMetalManager::Metals& spots =  circuit->GetGameAttribute()->GetMetalManager().GetSpots();
				CMetalManager::MetalIndices indices = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpots(position, 3);
				for (const int idx : indices) {
					buildPos = findBuildSite(buildDef, spots[idx].position, facing);
					if (buildPos != -RgtVector) {
						break;
					}
				}
			}
			if (buildPos != -RgtVector) {
				task->SetBuildPos(buildPos);
				u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_SHIFT_KEY);
			} else {
				task->MarkCompleted();
				builderTasks[CBuilderTask::TaskType::NANO].remove(task);
				isCachedChanged = true;
				delete task;
				builderTasksSize--;
				// Fallback to Guard/Assist
				assist(unit);
			}
			break;
		}
		case CBuilderTask::TaskType::EXPAND: {
			std::vector<float> params;
			params.push_back(2.0f);
			u->ExecuteCustomCommand(CMD_PRIORITY, params);

			if (task->GetTarget() != nullptr) {
				u->Repair(task->GetTarget()->GetUnit(), UNIT_COMMAND_OPTION_SHIFT_KEY);
				break;
			}
			UnitDef* buildDef = circuit->GetGameAttribute()->GetUnitDefByName("cormex");
			AIFloat3 buildPos = task->GetBuildPos();
			if (buildPos != -RgtVector && circuit->GetMap()->IsPossibleToBuildAt(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING)) {
				u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
				break;
			}
			AIFloat3 position = u->GetPos();
			const CMetalManager::Metals& spots = circuit->GetGameAttribute()->GetMetalManager().GetSpots();
			Map* map = circuit->GetMap();
			CMetalManager::MetalPredicate predicate = [&spots, map, buildDef](CMetalManager::MetalNode const& v) {
				return map->IsPossibleToBuildAt(buildDef, spots[v.second].position, UNIT_COMMAND_BUILD_NO_FACING);
			};
			int index = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpot(position, predicate);
			if (buildPos != -RgtVector) {
				const CMetalManager::Metals& spots =  circuit->GetGameAttribute()->GetMetalManager().GetSpots();
				buildPos = spots[index].position;
				task->SetBuildPos(buildPos);
				u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
			} else {
				task->MarkCompleted();
				builderTasks[CBuilderTask::TaskType::EXPAND].remove(task);
				isCachedChanged = true;
				delete task;
				builderTasksSize--;
				// Fallback to Guard/Assist
				assist(unit);
			}
			break;
		}
		case CBuilderTask::TaskType::ENERGIZE: {
			std::vector<float> params;
			params.push_back(1.0f);
			u->ExecuteCustomCommand(CMD_PRIORITY, params);

			if (task->GetTarget() != nullptr) {
				u->Repair(task->GetTarget()->GetUnit(), UNIT_COMMAND_OPTION_SHIFT_KEY);
				break;
			}
			UnitDef* buildDef = circuit->GetGameAttribute()->GetUnitDefByName("armsolar");
			AIFloat3 buildPos = task->GetBuildPos();
			if (buildPos != -RgtVector && circuit->GetMap()->IsPossibleToBuildAt(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING)) {
				u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
				break;
			}
			AIFloat3 position = task->GetPos();
			int facing;
			buildPos = findBuildSite(buildDef, position, facing);
			if (buildPos == -RgtVector) {
				// TODO: Replace FindNearestSpots with FindNearestClusters
				const CMetalManager::Metals& spots =  circuit->GetGameAttribute()->GetMetalManager().GetSpots();
				CMetalManager::MetalIndices indices = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpots(position, 3);
				for (const int idx : indices) {
					buildPos = findBuildSite(buildDef, spots[idx].position, facing);
					if (buildPos != -RgtVector) {
						break;
					}
				}
			}
			if (buildPos != -RgtVector) {
				task->SetBuildPos(buildPos);
				u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_SHIFT_KEY);
			} else {
				task->MarkCompleted();
				builderTasks[CBuilderTask::TaskType::ENERGIZE].remove(task);
				isCachedChanged = true;
				delete task;
				builderTasksSize--;
				// Fallback to Guard/Assist
				assist(unit);
			}
			break;
		}
		default:
		case CBuilderTask::TaskType::ASSIST: {
			assist(unit);
			break;
		}
	}
}

CEconomyManager::WorkerTaskRelation& CEconomyManager::GetWorkerTaskRelations(CCircuitUnit* unit, WorkerInfo*& retInfo)
{
	if (circuit->GetLastFrame() - cachedFrame < FRAMES_PER_SEC && !isCachedChanged) {
		for (auto info : wtRelation.front()) {
			if (info->unit == unit) {
				retInfo = info;
				break;
			}
		}
		return wtRelation;
	}

	utils::free_clear(wtRelation.front());
	wtRelation.clear();

	WorkerInfo* unitInfo;
	std::vector<WorkerInfo*> workerInfos;
	for (auto worker : workers) {
		WorkerInfo* info = new WorkerInfo;
		info->unit = worker;
		info->pos = worker->GetUnit()->GetPos();
		float speed = worker->GetUnit()->GetMaxSpeed();
		info->qspeed = speed * speed;
		// TODO: include buildtime
		workerInfos.push_back(info);

		if (worker == unit) {
			retInfo = info;
		}
	}
	auto qdist = [](const AIFloat3& p1, const AIFloat3& p2) {
		float x = p1.x - p2.x;
		float z = p1.z - p2.z;
		return x * x + z * z;
	};

	for (auto& tasks : builderTasks) {
		for (auto& task : tasks.second) {
			const AIFloat3& p0 = static_cast<CBuilderTask*>(task)->GetPos();
			auto compare = [&p0, qdist](const WorkerInfo* p1, const WorkerInfo* p2) {
				float t1 = qdist(p0, p1->pos) / p1->qspeed;
				float t2 = qdist(p0, p2->pos) / p2->qspeed;
				return t1 < t2;
			};
			std::sort(workerInfos.begin(), workerInfos.end(), compare);
			wtRelation.push_back(workerInfos);
		}
	}

	cachedFrame = circuit->GetLastFrame();
	isCachedChanged = false;
	return wtRelation;
}

} // namespace circuit
