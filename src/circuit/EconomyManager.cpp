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
#include "Economy.h"

// debug
#include "WrappTeam.h"
#include "TeamRulesParam.h"
#include "Drawer.h"

namespace circuit {

using namespace springai;

CEconomyManager::CEconomyManager(CCircuitAI* circuit) :
		IModule(circuit),
		totalBuildpower(.0f),
		cachedFrame(-1),
		isCachedChanged(true)
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
		utils::FreeClear(units);
		factories[unit] = nanos;

		AIFloat3 pos = u->GetPos();
		switch (u->GetBuildingFacing()) {
			case UNIT_FACING_SOUTH:
			default:
				pos.z += def->GetZSize() * 0.75 * SQUARE_SIZE;
				break;
			case UNIT_FACING_EAST:
				pos.x += def->GetXSize() * 0.75 * SQUARE_SIZE;
				break;
			case UNIT_FACING_NORTH:
				pos.z -= def->GetZSize() * 0.75 * SQUARE_SIZE;
				break;
			case UNIT_FACING_WEST:
				pos.x -= def->GetXSize() * 0.75 * SQUARE_SIZE;
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
		factories.erase(unit);
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

		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();
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
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
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
		this->totalBuildpower += unit->GetDef()->GetBuildSpeed();
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
		this->totalBuildpower -= unit->GetDef()->GetBuildSpeed();
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
	utils::FreeClear(factoryTasks);
	utils::FreeClear(builderTasks);
	utils::FreeClear(wtRelation.front());
	delete metalRes, energyRes;
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
						isCachedChanged = true;
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
					isCachedChanged = true;
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
	if (circuit->GetTeamId() != 1) {
		return;
	}
//	CBuilderTask* task = nullptr;
	int i = 0;
//	for (auto t : builderTasks) {
//		CBuilderTask* task = static_cast<CBuilderTask*>(t);
//		if (task->GetType() == CBuilderTask::TaskType::ENERGIZE) {
////			task = static_cast<CBuilderTask*>(t);
////			break;
//			i++;
//		}
//	}
//	if (i < 10) {
//		Map* map = circuit->GetMap();
//		int terWidth = map->GetWidth() * SQUARE_SIZE;
//		int terHeight = map->GetHeight() * SQUARE_SIZE;
//		float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
//		float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
//		AIFloat3 buildPos(x, map->GetElevationAt(x, z), z);
//		new CBuilderTask(CBuilderTask::Priority::LOW, buildPos, builderTasks, CBuilderTask::TaskType::ENERGIZE, 1);
//		new CBuilderTask(CBuilderTask::Priority::LOW, buildPos, builderTasks, CBuilderTask::TaskType::EXPAND, 1);
////		CCircuitUnit* commander = circuit->GetCommander();
////		if (commander) {
////			AIFloat3 buildPos = commander->GetUnit()->GetPos();
////			task = new CBuilderTask(CBuilderTask::Priority::LOW, 3, buildPos, builderTasks, CBuilderTask::TaskType::ENERGIZE);
////		}
//	}

	i = 0;
	for (auto t : builderTasks) {
		CBuilderTask* task = static_cast<CBuilderTask*>(t);
		if (task->GetType() == CBuilderTask::TaskType::FACTORY || task->GetType() == CBuilderTask::TaskType::NANO) {
			i++;
			break;
		}
	}
	static int a = 0;
//	if (i == 0 && totalBuildpower < metalIncome * 2) {
	if ((a++ % 30 == 0) && (builderTasks.size() < 10)) {
		CCircuitUnit* factory = nullptr;
		for (auto& fac : factories) {
			if (fac.second.size() < 3) {
				factory = fac.first;
				break;
			}
		}
		if (factory != nullptr) {
			AIFloat3 buildPos = factory->GetUnit()->GetPos();
			new CBuilderTask(CBuilderTask::Priority::LOW, buildPos, builderTasks, CBuilderTask::TaskType::NANO, 1);
			isCachedChanged = true;
		} else {
			Map* map = circuit->GetMap();
			int terWidth = map->GetWidth() * SQUARE_SIZE;
			int terHeight = map->GetHeight() * SQUARE_SIZE;
			float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
			float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
			AIFloat3 buildPos(x, map->GetElevationAt(x, z), z);
			new CBuilderTask(CBuilderTask::Priority::LOW, buildPos, builderTasks, CBuilderTask::TaskType::FACTORY, 1);
			isCachedChanged = true;
		}
	}

	if (a % 150 == 0) {
		Economy* eco = circuit->GetCallback()->GetEconomy();
		float metalIncome = eco->GetIncome(metalRes);
		delete eco;
		printf("metalIncome: %f\n", metalIncome);
		Team* team = WrappTeam::GetInstance(circuit->GetSkirmishAIId(), circuit->GetTeamId());
		std::vector<TeamRulesParam*> params = team->GetTeamRulesParams();
		for (auto param : params) {
			printf("Param: %s, value: %f\n", param->GetName(), param->GetValueFloat());
		}
		delete team;
		utils::FreeClear(params);

		for (auto t : builderTasks) {
			AIFloat3 pos = t->GetPos();
			circuit->GetDrawer()->DeletePointsAndLines(pos);
		}
		for (auto t : builderTasks) {
			AIFloat3 pos = t->GetPos();
			circuit->GetDrawer()->AddPoint(pos, "");
		}
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
	utils::FreeClear(units);
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
	// TODO: Refactor task picker. Task with min(dist*quantity) should be enough
	WorkerTaskRelation& wtRelation = GetWorkerTaskRelations(unit, unitInfo);
	int idx = wtRelation.front().size();
	int i = 0;
	for (auto& task : builderTasks) {
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
//	printf("workers size: %i\n", workers.size());
//	CBuilderTask* task = nullptr;
//	if (!builderTasks.empty()) {
//		task = static_cast<CBuilderTask*>(builderTasks.front());
//	}

	if (task == nullptr) {
		AIFloat3 pos = unit->GetUnit()->GetPos();
//		task = new CBuilderTask(CBuilderTask::Priority::LOW, 3, pos, builderTasks, CBuilderTask::TaskType::ASSIST, 60, FRAMES_PER_SEC * 60);
		task = new CBuilderTask(CBuilderTask::Priority::LOW, pos, builderTasks, CBuilderTask::TaskType::ENERGIZE, 1);
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
				isCachedChanged = true;
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
				isCachedChanged = true;
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
			CMetalManager::Metal spot = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpot(position, predicate);
			buildPos = spot.position;
			if (buildPos != -RgtVector) {
				task->SetBuildPos(buildPos);
				u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING, UNIT_COMMAND_OPTION_SHIFT_KEY);
			} else {
				task->MarkCompleted();
				isCachedChanged = true;
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
				isCachedChanged = true;
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

		if (worker == unit) {
			retInfo = info;
		}
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
	isCachedChanged = false;
	return wtRelation;
}

} // namespace circuit
