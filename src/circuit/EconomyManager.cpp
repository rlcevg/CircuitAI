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
		totalBuildpower(.0f),
		cachedFrame(-1)
{
	metalRes = circuit->GetCallback()->GetResourceByName("Metal");
	energyRes = circuit->GetCallback()->GetResourceByName("Energy");
	std::vector<WorkerInfo*> wi;
	wtRelation.push_back(wi);

	// TODO: Use A* ai planning... or sth... STRIPS https://ru.wikipedia.org/wiki/STRIPS
	//       https://ru.wikipedia.org/wiki/Марковский_процесс_принятия_решений
	CGameAttribute* attrib = circuit->GetGameAttribute();
	int unitDefId;

	SkirmishAIs* ais = circuit->GetCallback()->GetSkirmishAIs();
	int aisCount = ais->GetSize();
	delete ais;
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::Update, this), aisCount, circuit->GetSkirmishAIId());
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
		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();
		this->workers.insert(unit);

//		PrepareBuilder(unit);
//		ExecuteBuilder(unit);
	};
	idleHandler[unitDefId] = [this](CCircuitUnit* unit) {
		unit->RemoveTask();
		PrepareBuilder(unit);
		ExecuteBuilder(unit);
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
		this->workers.erase(unit);
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
	utils::FreeClear(factoryTasks);
	utils::FreeClear(builderTasks);
	utils::FreeClear(wtRelation.front());
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
						taskF->MarkCompleted();  // task will remove itself from owner on MarkCompleted
						for (auto u : units) {
							unfinishedUnits[u] = nullptr;
						}
						unfinishedTasks.erase(task);
						delete task;
					} else {
						units.remove(unit);
					}
					break;
				}
				case IConstructTask::ConstructType::BUILDER: {
					CBuilderTask* taskB = static_cast<CBuilderTask*>(task);
					taskB->MarkCompleted();  // task will remove itself from owner on MarkCompleted
					delete task;
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

void CEconomyManager::Update()
{
//	CBuilderTask* task = nullptr;
	int i = 0;
	for (auto t : builderTasks) {
		CBuilderTask* task = static_cast<CBuilderTask*>(t);
		if (task->GetType() == CBuilderTask::TaskType::ENERGIZE) {
//			task = static_cast<CBuilderTask*>(t);
//			break;
			i++;
		}
	}

	if (i < 10) {
		Map* map = circuit->GetMap();
		int terWidth = map->GetWidth() * SQUARE_SIZE;
		int terHeight = map->GetHeight() * SQUARE_SIZE;
		float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
		float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
		AIFloat3 buildPos(x, map->GetElevationAt(x, z), z);
		new CBuilderTask(CBuilderTask::Priority::LOW, buildPos, builderTasks, CBuilderTask::TaskType::ENERGIZE, 1);
		new CBuilderTask(CBuilderTask::Priority::LOW, buildPos, builderTasks, CBuilderTask::TaskType::EXPAND, 1);
//		CCircuitUnit* commander = circuit->GetCommander();
//		if (commander) {
//			AIFloat3 buildPos = commander->GetUnit()->GetPos();
//			task = new CBuilderTask(CBuilderTask::Priority::LOW, 3, buildPos, builderTasks, CBuilderTask::TaskType::ENERGIZE);
//		}
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
		task = new CFactoryTask(CFactoryTask::Priority::LOW, buildPos, factoryTasks, CFactoryTask::TaskType::DEFAULT, radius, 2);
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
	auto qdist = [](const AIFloat3& p1, const AIFloat3& p2) {
		float x = p1.x - p2.x;
		float z = p1.z - p2.z;
		return x * x + z * z;
	};

	std::vector<CBuilderTask*> candidates;
	WorkerInfo* unitInfo;
	// TODO: Refactor task picker? Maybe task with min(dist*quantity) is enough
	WorkerTaskRelation& wtRelation = GetWorkerTaskRelations(unit, unitInfo);
	int idx = wtRelation.front().size();
	int i = 0;
	for (auto task : builderTasks) {
		if (task->CanAssignTo(unit)) {
			CBuilderTask* candidate = static_cast<CBuilderTask*>(task);
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

	CBuilderTask* task = nullptr;
	if (!candidates.empty()) {
		task = candidates.front();
		float dist = qdist(task->GetPos(), unitInfo->pos);
		int quantity = task->GetQuantity();
		for (auto t : candidates) {
			float d = qdist(t->GetPos(), unitInfo->pos);
			int q = t->GetQuantity();
			if ((q < quantity) || (q == quantity && d < dist)) {
//			if ((q < quantity) || (q * d < quantity * dist)) {
				quantity = q;
				dist = d;
				task = t;
			}
		}
	}

	if (task == nullptr) {
		AIFloat3 pos = unit->GetUnit()->GetPos();
//		task = new CBuilderTask(CBuilderTask::Priority::LOW, 3, pos, builderTasks, CBuilderTask::TaskType::ASSIST, 60, FRAMES_PER_SEC * 60);
		task = new CBuilderTask(CBuilderTask::Priority::LOW, pos, builderTasks, CBuilderTask::TaskType::ENERGIZE, 1);
	}

	task->AssignTo(unit);
}

void CEconomyManager::ExecuteBuilder(CCircuitUnit* unit)
{
	CBuilderTask* task = static_cast<CBuilderTask*>(unit->GetTask());
	Unit* u = unit->GetUnit();

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
		return circuit->FindBuildSiteMindMex(buildDef, position, 800.0f, facing);
	};
	auto assist = [this](CCircuitUnit* unit) {
		CCircuitUnit* target = FindUnitToAssist(unit);
		unit->GetUnit()->Repair(target->GetUnit(), UNIT_COMMAND_OPTION_SHIFT_KEY);
		builderInfo[unit].startFrame = circuit->GetLastFrame();
	};

	switch (task->GetType()) {
		case CBuilderTask::TaskType::BUILD: {
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
			if (buildPos != -RgtVector) {
				u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
				break;
			}
			AIFloat3 position = u->GetPos();
			int idx = circuit->GetGameAttribute()->GetMetalManager().FindNearestOpenSpotIndex(position, circuit->GetAllyTeamId());
			if (idx >= 0) {
				const CMetalManager::Metal& spot = circuit->GetGameAttribute()->GetMetalManager()[idx];
				buildPos = spot.position;
				task->SetBuildPos(buildPos);
				u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
			} else {
				task->MarkCompleted();
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
			if (buildPos != -RgtVector) {
				u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
				break;
			}
			AIFloat3 position = task->GetPos();
			buildPos = findBuildSite(buildDef, position);
			if (buildPos == -RgtVector) {
				// TODO: Replace FindNearestSpots with FindNearestClusters
				CMetalManager::Metals spots = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpots(position, 3);
				for (auto& spot : spots) {
					buildPos = findBuildSite(buildDef, spot.position);
					if (buildPos != -RgtVector) {
						break;
					}
				}
			}
			if (buildPos != -RgtVector) {
				task->SetBuildPos(buildPos);
				u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
			} else {
				task->MarkCompleted();
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
	if (cachedFrame == circuit->GetLastFrame()) {
		for (auto info : wtRelation.front()) {
			if (info->unit == unit) {
				retInfo = info;
				break;
			}
		}
		return wtRelation;
	}

	utils::FreeClear(wtRelation.front());
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
	}
	auto qdist = [](const AIFloat3& p1, const AIFloat3& p2) {
		float x = p1.x - p2.x;
		float z = p1.z - p2.z;
		return x * x + z * z;
	};

	for (auto task : builderTasks) {
		AIFloat3& p0 = static_cast<CBuilderTask*>(task)->GetPos();
		auto compare = [&p0, qdist](const WorkerInfo* p1, const WorkerInfo* p2) {
			float t1 = qdist(p0, p1->pos) / p1->qspeed;
			float t2 = qdist(p0, p2->pos) / p2->qspeed;
			return t1 < t2;
		};
		std::sort(workerInfos.begin(), workerInfos.end(), compare);
		wtRelation.push_back(workerInfos);
	}

	cachedFrame = circuit->GetLastFrame();
	for (auto info : wtRelation.front()) {
		if (info->unit == unit) {
			retInfo = info;
			break;
		}
	}
	return wtRelation;
}

} // namespace circuit
