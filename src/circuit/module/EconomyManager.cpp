/*
 * EconomyManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "module/EconomyManager.h"
#include "CircuitAI.h"
#include "static/SetupManager.h"
#include "static/MetalManager.h"
#include "unit/CircuitUnit.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "terrain/TerrainManager.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Map.h"
#include "SkirmishAIs.h"
#include "Resource.h"
#include "Economy.h"
#include "Feature.h"

namespace circuit {

using namespace springai;

CEconomyManager::CEconomyManager(CCircuitAI* circuit) :
		IModule(circuit),
		pylonCount(0)
{
	metalRes = circuit->GetCallback()->GetResourceByName("Metal");
	energyRes = circuit->GetCallback()->GetResourceByName("Energy");
	eco = circuit->GetCallback()->GetEconomy();

	UnitDef* def = circuit->GetUnitDefByName("armestor");
	const std::map<std::string, std::string>& customParams = def->GetCustomParams();
	auto search = customParams.find("pylonrange");
	pylonRange = (search != customParams.end()) ? utils::string_to_float(search->second) : 500;

	// TODO: Use A* ai planning... or sth... STRIPS https://ru.wikipedia.org/wiki/STRIPS
	//       https://ru.wikipedia.org/wiki/Марковский_процесс_принятия_решений

	circuit->GetScheduler()->RunParallelTask(CGameTask::emptyTask, std::make_shared<CGameTask>(&CEconomyManager::Init, this));

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

	int unitDefId;

	/*
	 * factorycloak handlers
	 */
	unitDefId = circuit->GetUnitDefByName("factorycloak")->GetUnitDefId();
	finishedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		// check factory's cluster
		int index = this->circuit->GetMetalManager()->FindNearestCluster(unit->GetUnit()->GetPos());
		if (index >= 0) {
			clusterInfos[index].factory = unit;
		}
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		for (auto& info : clusterInfos) {
			if (info.factory == unit) {
				info.factory = nullptr;
			}
		}
	};

	/*
	 * armestor handlers
	 */
	unitDefId = circuit->GetUnitDefByName("armestor")->GetUnitDefId();
	createdHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		// check pylon's cluster
		int index = this->circuit->GetMetalManager()->FindNearestCluster(unit->GetUnit()->GetPos());
		if (index >= 0) {
			clusterInfos[index].pylon = unit;
		}
		pylonCount++;
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		for (auto& info : clusterInfos) {
			if (info.pylon == unit) {
				info.pylon = nullptr;
			}
		}
		pylonCount--;
	};
	Map* map = circuit->GetMap();
	float pylonSquare = pylonRange * 2 / SQUARE_SIZE;
	pylonSquare *= pylonSquare;
	pylonMaxCount = ((map->GetWidth() * map->GetHeight()) / pylonSquare) / 2;

	/*
	 * Identify resource buildings
	 */
	// ZK doesn't use unitDef's extractsMetal/metalMake, but has customParams::ismex = 1. Anyway there is only 1 mex type.
	mexDef = circuit->GetUnitDefByName("cormex");

//	CCircuitAI::UnitDefs& defs = circuit->GetUnitDefs();
//	for (auto& kv : defs) {
//		UnitDef* def = kv.second;
//		if (def->GetSpeed() <= 0) {
//			float make = def->GetResourceMake(energyRes);
//			float use = def->GetUpkeep(energyRes);
//			if ((make > 0) || (use < 0)) {
//				Energy engy;
//				engy.def = def;
//				engy.make = (make > 0) ? make : -use;
//				engy.cost = def->GetCost(metalRes);
//				energyDefs.push_back(engy);
//			}
//		}
//	}

	Energy engy;

	engy.def = circuit->GetUnitDefByName("armsolar");
	engy.make = 2;
	engy.cost = engy.def->GetCost(metalRes);
	energyDefs.push_back(engy);

//	engy.def = circuit->GetUnitDefByName("armwin");  // Tidal
//	engy.make = engy.def->GetTidalResourceGenerator(energyRes) * circuit->GetMap()->GetTidalStrength();
//	engy.cost = engy.def->GetCost(metalRes);
//	energyDefs.push_back(engy);

//	engy.def = circuit->GetUnitDefByName("armwin");  // Wind
//	engy.make = engy.def->GetWindResourceGenerator(energyRes) * circuit->GetMap()->GetMinWind();
//	engy.cost = engy.def->GetCost(metalRes);
//	energyDefs.push_back(engy);

	engy.def = circuit->GetUnitDefByName("armfus");
	engy.make = engy.def->GetResourceMake(energyRes);
	engy.cost = engy.def->GetCost(metalRes);
	energyDefs.push_back(engy);

	engy.def = circuit->GetUnitDefByName("cafus");
	engy.make = engy.def->GetResourceMake(energyRes);
	engy.cost = engy.def->GetCost(metalRes);
	energyDefs.push_back(engy);

	// High-tech energy first
	auto compare = [](const Energy& e1, const Energy& e2) {
		return (e1.make / e1.cost) > (e2.make / e2.cost);
	};
	std::sort(energyDefs.begin(), energyDefs.end(), compare);
}

CEconomyManager::~CEconomyManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete metalRes, energyRes, eco;
}

int CEconomyManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitFinished(CCircuitUnit* unit)
{
	auto search = finishedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	auto search = destroyedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

IBuilderTask* CEconomyManager::CreateBuilderTask(CCircuitUnit* unit)
{
	// TODO: Add general logic here
	Unit* u = unit->GetUnit();
	const AIFloat3& pos = u->GetPos();

	IBuilderTask* task;
	task = UpdateMetalTasks(pos);
	if (task != nullptr) {
		return task;
	}
	task = UpdateEnergyTasks(pos);
	if (task != nullptr) {
		return task;
	}
	task = UpdateFactoryTasks(pos);
	if (task != nullptr) {
		return task;
	}

	CBuilderManager* builderManager = circuit->GetBuilderManager();
	std::vector<Feature*> features = circuit->GetCallback()->GetFeaturesIn(pos, u->GetMaxSpeed() * FRAMES_PER_SEC * 60);
	if (!features.empty() && (builderManager->GetTasks(IBuilderTask::BuildType::RECLAIM).size() < 20)) {
		task = builderManager->EnqueueTask(IBuilderTask::Priority::LOW, pos, IBuilderTask::BuildType::RECLAIM, FRAMES_PER_SEC * 60);
	}
	utils::free_clear(features);
	if (task != nullptr) {
		return task;
	}

	const std::set<IBuilderTask*>& tasks = builderManager->GetTasks(IBuilderTask::BuildType::BIG_GUN);
	if (tasks.empty()) {
		task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, circuit->GetUnitDefByName("raveparty"),
										   circuit->GetSetupManager()->GetStartPos(), IBuilderTask::BuildType::BIG_GUN);
	} else {
		task = *tasks.begin();
	}
	return task;
}

CRecruitTask* CEconomyManager::CreateFactoryTask(CCircuitUnit* unit)
{
	// TODO: Add general logic here
	CRecruitTask* task = UpdateRecruitTasks();
	if (task != nullptr) {
		return task;
	}

	Unit* u = unit->GetUnit();
	UnitDef* def = unit->GetDef();
	const char* names3[] = {"armrock", "armpw", "armwar", "armsnipe", "armjeth", "armzeus"};
	const char* names2[] = {"armpw", "armrock", "armpw", "armwar", "armsnipe", "armzeus"};
	const char* names1[] = {"armpw", "armrock", "armpw", "armwar", "armpw", "armrock"};
	char** names;
	float metalIncome = eco->GetIncome(metalRes);
	if (metalIncome > 30) {
		names = (char**)names3;
	} else if (metalIncome > 20) {
		names = (char**)names2;
	} else {
		names = (char**)names1;
	}
	UnitDef* buildDef = circuit->GetUnitDefByName(names[rand() % 6]);
	const AIFloat3& buildPos = u->GetPos();
	float radius = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE * 4;
	task = circuit->GetFactoryManager()->EnqueueTask(CRecruitTask::Priority::LOW, buildDef, buildPos, CRecruitTask::FacType::DEFAULT, 1, radius);
	return task;
}

Resource* CEconomyManager::GetMetalRes() const
{
	return metalRes;
}

Resource* CEconomyManager::GetEnergyRes() const
{
	return energyRes;
}

UnitDef* CEconomyManager::GetMexDef() const
{
	return mexDef;
}

AIFloat3 CEconomyManager::FindBuildPos(CCircuitUnit* unit)
{
	IBuilderTask* task = static_cast<IBuilderTask*>(unit->GetTask());
	Unit* u = unit->GetUnit();
	UnitDef* buildDef = task->GetBuildDef();
	AIFloat3 buildPos = -RgtVector;
	CMetalManager* metalManager = circuit->GetMetalManager();
	switch (task->GetBuildType()) {
		case IBuilderTask::BuildType::MEX: {
			const AIFloat3& position = u->GetPos();
			const CMetalData::Metals& spots = metalManager->GetSpots();
			const std::vector<CMetalManager::MetalInfo>& metalInfos = metalManager->GetMetalInfos();
			Map* map = circuit->GetMap();
			CMetalData::MetalPredicate predicate = [&spots, &metalInfos, map, buildDef](CMetalData::MetalNode const& v) {
				int index = v.second;
				return (metalInfos[index].isOpen && map->IsPossibleToBuildAt(buildDef, spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
			};
			int index = metalManager->FindNearestSpot(position, predicate);
			if (index >= 0) {
				buildPos = spots[index].position;
			}
			break;
		}
		case IBuilderTask::BuildType::PYLON: {
			const AIFloat3& position = task->GetPos();
			CTerrainManager* terrain = circuit->GetTerrainManager();
			buildPos = terrain->FindBuildSite(buildDef, position, pylonRange * 16, UNIT_COMMAND_BUILD_NO_FACING);
			if (buildPos == -RgtVector) {
				const CMetalData::Metals& spots = metalManager->GetSpots();
				CMetalData::MetalPredicate predicate = [this](const CMetalData::MetalNode& v) {
					return clusterInfos[v.second].pylon == nullptr;
				};
				CMetalData::MetalIndices indices = metalManager->FindNearestClusters(position, 3, predicate);
				for (const int idx : indices) {
					buildPos = terrain->FindBuildSite(buildDef, spots[idx].position, pylonRange * 16, UNIT_COMMAND_BUILD_NO_FACING);
					if (buildPos != -RgtVector) {
						break;
					}
				}
			}
			break;
		}
	}
	return buildPos;
}

void CEconomyManager::Init()
{
	const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
	clusterInfos.resize(clusters.size());
	for (int i = 0; i < clusters.size(); i++) {
		clusterInfos[i] = {nullptr};
	}

	SkirmishAIs* ais = circuit->GetCallback()->GetSkirmishAIs();
	const int interval = ais->GetSize() * 2;
	delete ais;
	const AIFloat3& pos = circuit->GetSetupManager()->GetStartPos();
	CScheduler* scheduler = circuit->GetScheduler();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateFactoryTasks, this, pos), interval, circuit->GetSkirmishAIId() + 0 + 10 * interval);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateStorageTasks, this), interval, circuit->GetSkirmishAIId() + 1);
}

IBuilderTask* CEconomyManager::UpdateMetalTasks(const AIFloat3& position)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	IBuilderTask* task = nullptr;
	if (!builderManager->CanEnqueueTask()) {
		return task;
	}

	// check uncolonized mexes
	float energyIncome = eco->GetIncome(energyRes);
	float metalIncome = eco->GetIncome(metalRes);
	if ((energyIncome * 0.8 > metalIncome) && circuit->IsAvailable(mexDef)) {
		float cost = mexDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 3 + 1;
		if (builderManager->GetTasks(IBuilderTask::BuildType::MEX).size() < count) {
			CMetalManager* metalManager = circuit->GetMetalManager();
			const CMetalData::Metals& spots = metalManager->GetSpots();
			const std::vector<CMetalManager::MetalInfo>& metalInfos = metalManager->GetMetalInfos();
			Map* map = circuit->GetMap();
			UnitDef* metalDef = mexDef;
			CMetalData::MetalPredicate predicate = [&spots, &metalInfos, map, metalDef](CMetalData::MetalNode const& v) {
				int index = v.second;
				return (metalInfos[index].isOpen && map->IsPossibleToBuildAt(metalDef, spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
			};
			int index = metalManager->FindNearestSpot(position, predicate);
			if (index != -1) {
				metalManager->SetOpenSpot(index, false);
				const AIFloat3& pos = spots[index].position;
				task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, mexDef, pos, IBuilderTask::BuildType::MEX, cost);
				task->SetBuildPos(pos);
				return task;
			}
		}
	}

	return task;
}

IBuilderTask* CEconomyManager::UpdateEnergyTasks(const AIFloat3& position)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	IBuilderTask* task = nullptr;
	if (!builderManager->CanEnqueueTask()) {
		return task;
	}

	// TODO: Average income
	// check energy / metal ratio
	float energyIncome = eco->GetIncome(energyRes);
	float metalIncome = eco->GetIncome(metalRes);
	float energyUsage = eco->GetUsage(energyRes);

	if ((metalIncome > energyIncome * 0.8) || (energyUsage > energyIncome * 0.8)) {
		UnitDef* bestDef = nullptr;
		float cost;
		float buildPower = std::min(builderManager->GetBuilderPower(), metalIncome * 0.5);
		const std::set<IBuilderTask*>& tasks = builderManager->GetTasks(IBuilderTask::BuildType::ENERGY);
		for (auto& engy : energyDefs) {  // sorted by high-tech first
			if (!circuit->IsAvailable(engy.def)) {
				continue;
			}
			// TODO: Select proper scale/quadratic function (x*x) and smoothing coefficient (8)
			float metric = engy.cost / (buildPower * buildPower / 8);
			if (metric < MAX_BUILD_SEC) {
				int count = buildPower / engy.cost * 4 + 1;
				if (tasks.size() < count) {
					cost = engy.cost;
					bestDef = engy.def;
				}
				break;
			}
		}

		if (bestDef != nullptr) {
			AIFloat3 buildPos = -RgtVector;

			CMetalManager* metalManager = circuit->GetMetalManager();
			if (cost / std::min(builderManager->GetBuilderPower(), metalIncome) < MIN_BUILD_SEC) {
				int index = metalManager->FindNearestSpot(position);
				if (index != -1) {
					const CMetalData::Metals& spots = metalManager->GetSpots();
					buildPos = spots[index].position;
				}
			} else {
				const AIFloat3& startPos = circuit->GetSetupManager()->GetStartPos();
				int index = metalManager->FindNearestCluster(startPos);
				if (index >= 0) {
					const CMetalData::Clusters& clusters = metalManager->GetClusters();
					buildPos = clusters[index].geoCentr;
				}
			}

			if (buildPos == -RgtVector) {
				CTerrainManager* terrain = circuit->GetTerrainManager();
				int terWidth = terrain->GetTerrainWidth();
				int terHeight = terrain->GetTerrainHeight();
				float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
				float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
				buildPos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
			}

			task = builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, bestDef, buildPos, IBuilderTask::BuildType::ENERGY, cost);
			return task;
		}
	}

	return task;
}

IBuilderTask* CEconomyManager::UpdateFactoryTasks(const AIFloat3& position)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	IBuilderTask* task = nullptr;
	if (!builderManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* assistDef = circuit->GetUnitDefByName("armnanotc");
	UnitDef* facDef = circuit->GetUnitDefByName("factorycloak");

	// check buildpower
	float metalIncome = eco->GetIncome(metalRes);
	CFactoryManager* factoryManager = circuit->GetFactoryManager();
	if ((factoryManager->GetFactoryPower() < metalIncome) &&
			builderManager->GetTasks(IBuilderTask::BuildType::FACTORY).empty() && builderManager->GetTasks(IBuilderTask::BuildType::NANO).empty()) {
		CCircuitUnit* factory = factoryManager->NeedUpgrade();
		if ((factory != nullptr) && circuit->IsAvailable(assistDef)) {
			Unit* u = factory->GetUnit();
			UnitDef* def = factory->GetDef();
			AIFloat3 buildPos = u->GetPos();
			switch (u->GetBuildingFacing()) {
				default:
				case UNIT_FACING_SOUTH:
					buildPos.z -= def->GetZSize() * SQUARE_SIZE;
					break;
				case UNIT_FACING_EAST:
					buildPos.x -= def->GetXSize() * SQUARE_SIZE;
					break;
				case UNIT_FACING_NORTH:
					buildPos.z += def->GetZSize() * SQUARE_SIZE;
					break;
				case UNIT_FACING_WEST:
					buildPos.x += def->GetXSize() * SQUARE_SIZE;
					break;
			}
			task = builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, assistDef, buildPos, IBuilderTask::BuildType::NANO);
		} else if (circuit->IsAvailable(facDef)) {
			CMetalData::MetalPredicate predicate = [this](const CMetalData::MetalNode& v) {
				return clusterInfos[v.second].factory == nullptr;
			};
			CMetalManager* metalManager = circuit->GetMetalManager();
			int index = metalManager->FindNearestCluster(position, predicate);
			CTerrainManager* terrain = circuit->GetTerrainManager();
			AIFloat3 buildPos;
			if (index >= 0) {
				const CMetalData::Clusters& clusters = metalManager->GetClusters();
				buildPos = clusters[index].geoCentr;
				float size = std::max(facDef->GetXSize(), facDef->GetZSize()) * SQUARE_SIZE;
				buildPos.x += (buildPos.x > terrain->GetTerrainWidth() / 2) ? -size : size;
				buildPos.z += (buildPos.z > terrain->GetTerrainHeight() / 2) ? -size : size;
			} else {
				int terWidth = terrain->GetTerrainWidth();
				int terHeight = terrain->GetTerrainHeight();
				float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
				float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
				buildPos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
			}
			task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, facDef, buildPos, IBuilderTask::BuildType::FACTORY);
		}
	}

	return task;
}

CRecruitTask* CEconomyManager::UpdateRecruitTasks()
{
	CFactoryManager* factoryManager = circuit->GetFactoryManager();
	CRecruitTask* task = nullptr;
	if (!factoryManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* buildDef = circuit->GetUnitDefByName("armrectr");

	float metalIncome = eco->GetIncome(metalRes);
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	// TODO: Create ReclaimTask for 20% of workers, and 20% RepairTask.
	if ((builderManager->GetBuilderPower() < metalIncome * 1.2) && circuit->IsAvailable(buildDef)) {
		for (auto t : factoryManager->GetTasks()) {
			if (t->GetFacType() == CRecruitTask::FacType::BUILDPOWER) {
				return task;
			}
		}
		CCircuitUnit* factory = factoryManager->GetRandomFactory();
		const AIFloat3& buildPos = factory->GetUnit()->GetPos();
		CTerrainManager* terrain = circuit->GetTerrainManager();
		float radius = std::max(terrain->GetTerrainWidth(), terrain->GetTerrainHeight()) / 4;
		task = factoryManager->EnqueueTask(CRecruitTask::Priority::NORMAL, buildDef, buildPos, CRecruitTask::FacType::BUILDPOWER, 1, radius);
	}

	return task;
}

IBuilderTask* CEconomyManager::UpdateStorageTasks()
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	IBuilderTask* task = nullptr;
	if (!builderManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* storeDef = circuit->GetUnitDefByName("armmstor");
	UnitDef* pylonDef = circuit->GetUnitDefByName("armestor");

	float income = eco->GetIncome(metalRes);
	float storage = eco->GetStorage(metalRes);
	if (builderManager->GetTasks(IBuilderTask::BuildType::STORE).empty() && (storage / income < 25) && circuit->IsAvailable(storeDef)) {
		const AIFloat3& startPos = circuit->GetSetupManager()->GetStartPos();
		CMetalManager* metalManager = circuit->GetMetalManager();
		int index = metalManager->FindNearestSpot(startPos);
		AIFloat3 buildPos;
		if (index != -1) {
			const CMetalData::Metals& spots = metalManager->GetSpots();
			buildPos = spots[index].position;
		} else {
			CTerrainManager* terrain = circuit->GetTerrainManager();
			int terWidth = terrain->GetTerrainWidth();
			int terHeight = terrain->GetTerrainHeight();
			float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
			float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
			buildPos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
		}
		task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, storeDef, buildPos, IBuilderTask::BuildType::STORE);
		return task;
	}

	if ((income > 30) && (pylonCount < pylonMaxCount) && circuit->IsAvailable(pylonDef)) {
		float cost = pylonDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 2 + 1;
		if (builderManager->GetTasks(IBuilderTask::BuildType::PYLON).size() < count) {
//			CMetalData::MetalPredicate predicate = [this](const CMetalData::MetalNode& v) {
//				return clusterInfos[v.second].pylon == nullptr;
//			};
			CMetalManager* metalManager = circuit->GetMetalManager();
			const AIFloat3& startPos = circuit->GetSetupManager()->GetStartPos();
			int index = metalManager->FindNearestCluster(startPos/*, predicate*/);
			if (index >= 0) {
				const CMetalData::Clusters& clusters = metalManager->GetClusters();
				task = builderManager->EnqueueTask(IBuilderTask::Priority::LOW, pylonDef, clusters[index].geoCentr, IBuilderTask::BuildType::PYLON);
			}
			return task;
		}
	}

	return task;
}

} // namespace circuit
