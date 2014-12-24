/*
 * EconomyManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "EconomyManager.h"
#include "CircuitAI.h"
#include "Scheduler.h"
#include "SetupManager.h"
#include "MetalManager.h"
#include "CircuitUnit.h"
#include "BuilderManager.h"
#include "FactoryManager.h"
#include "TerrainManager.h"
#include "utils.h"

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
		solarCount(0),
		fusionCount(0),
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

	circuit->GetScheduler()->RunParallelTask(CGameTask::EmptyTask, std::make_shared<CGameTask>(&CEconomyManager::Init, this));

	// TODO: Group handlers
	//       Scout:
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
	 * armsolar handlers
	 */
	unitDefId = circuit->GetUnitDefByName("armsolar")->GetUnitDefId();
	createdHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		solarCount++;
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		solarCount--;
	};

	/*
	 * armfus handlers
	 */
	unitDefId = circuit->GetUnitDefByName("armfus")->GetUnitDefId();
	createdHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		fusionCount++;
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		fusionCount--;
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

CBuilderTask* CEconomyManager::CreateBuilderTask(CCircuitUnit* unit)
{
	// TODO: Add general logic here
	Unit* u = unit->GetUnit();
	const AIFloat3& pos = u->GetPos();

	CBuilderTask* task;
	task = UpdateMetalTasks(pos);
	if (task != nullptr) {
		return task;
	}
	task = UpdateEnergyTasks(pos);
	if (task != nullptr) {
		return task;
	}
	task = UpdateBuilderTasks(pos);
	if (task != nullptr) {
		return task;
	}

	CBuilderManager* builderManager = circuit->GetBuilderManager();
	std::vector<Feature*> features = circuit->GetCallback()->GetFeaturesIn(pos, u->GetMaxSpeed() * FRAMES_PER_SEC * 60);
	if (!features.empty() && (builderManager->GetTasks(CBuilderTask::TaskType::RECLAIM).size() < 20)) {
		task = builderManager->EnqueueTask(CBuilderTask::Priority::LOW, pos, CBuilderTask::TaskType::RECLAIM, FRAMES_PER_SEC * 60);
	}
	utils::free_clear(features);
	if (task != nullptr) {
		return task;
	}

	const std::list<CBuilderTask*>& tasks = builderManager->GetTasks(CBuilderTask::TaskType::RAVE);
	if (tasks.empty()) {
		task = builderManager->EnqueueTask(CBuilderTask::Priority::LOW, circuit->GetUnitDefByName("raveparty"), circuit->GetSetupManager()->GetStartPos(), CBuilderTask::TaskType::RAVE);
	} else {
		task = tasks.front();
	}
	return task;
}

CFactoryTask* CEconomyManager::CreateFactoryTask(CCircuitUnit* unit)
{
	// TODO: Add general logic here
	CFactoryTask* task = UpdateFactoryTasks();
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
	task = circuit->GetFactoryManager()->EnqueueTask(CFactoryTask::Priority::LOW, buildDef, buildPos, CFactoryTask::TaskType::DEFAULT, 1, radius);
	return task;
}

Resource* CEconomyManager::GetMetalRes()
{
	return metalRes;
}

Resource* CEconomyManager::GetEnergyRes()
{
	return energyRes;
}

AIFloat3 CEconomyManager::FindBuildPos(CCircuitUnit* unit)
{
	CBuilderTask* task = static_cast<CBuilderTask*>(unit->GetTask());
	Unit* u = unit->GetUnit();
	UnitDef* buildDef = task->GetBuildDef();
	AIFloat3 buildPos = -RgtVector;
	CMetalManager* metalManager = circuit->GetMetalManager();
	switch (task->GetType()) {
		case CBuilderTask::TaskType::EXPAND: {
			const AIFloat3& position = u->GetPos();
			const CMetalData::Metals& spots = metalManager->GetSpots();
			const std::vector<CMetalManager::MetalInfo>& metalInfos = metalManager->GetMetalInfos();
			Map* map = circuit->GetMap();
			CMetalData::MetalPredicate predicate = [&spots, &metalInfos, map, buildDef](CMetalData::MetalNode const& v) {
				int index = v.second;
				return (metalInfos[index].open && map->IsPossibleToBuildAt(buildDef, spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
			};
			int index = metalManager->FindNearestSpot(position, predicate);
			if (index >= 0) {
				buildPos = spots[index].position;
			}
			break;
		}
		case CBuilderTask::TaskType::PYLON: {
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
	const std::vector<CMetalData::MetalIndices>& clusters = circuit->GetMetalManager()->GetClusters();
	clusterInfos.resize(clusters.size());
	for (int i = 0; i < clusters.size(); i++) {
		clusterInfos[i] = {nullptr};
	}

	SkirmishAIs* ais = circuit->GetCallback()->GetSkirmishAIs();
	const int interval = ais->GetSize() * 2;
	delete ais;
	const AIFloat3& pos = circuit->GetSetupManager()->GetStartPos();
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateBuilderTasks, this, pos), interval, circuit->GetSkirmishAIId() + 0 + 10 * interval);
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateStorageTasks, this), interval, circuit->GetSkirmishAIId() + 1);
}

CBuilderTask* CEconomyManager::UpdateMetalTasks(const AIFloat3& position)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	CBuilderTask* task = nullptr;
	if (!builderManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* metalDef = circuit->GetMexDef();  // cormex

	// check uncolonized mexes
	float energyIncome = eco->GetIncome(energyRes);
	float metalIncome = eco->GetIncome(metalRes);
	if ((energyIncome * 0.8 > metalIncome) && circuit->IsAvailable(metalDef)) {
		float cost = metalDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 3 + 1;
		if (builderManager->GetTasks(CBuilderTask::TaskType::EXPAND).size() < count) {
			CMetalManager* metalManager = circuit->GetMetalManager();
			const CMetalData::Metals& spots = metalManager->GetSpots();
			const std::vector<CMetalManager::MetalInfo>& metalInfos = metalManager->GetMetalInfos();
			Map* map = circuit->GetMap();
			CMetalData::MetalPredicate predicate = [&spots, &metalInfos, map, metalDef](CMetalData::MetalNode const& v) {
				int index = v.second;
				return (metalInfos[index].open && map->IsPossibleToBuildAt(metalDef, spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
			};
			int index = metalManager->FindNearestSpot(position, predicate);
			if (index != -1) {
				metalManager->SetOpenSpot(index, false);
				const AIFloat3& pos = spots[index].position;
				task = builderManager->EnqueueTask(CBuilderTask::Priority::HIGH, metalDef, pos, CBuilderTask::TaskType::EXPAND, cost);
				task->SetBuildPos(pos);
				// TODO: Make separate defence task updater?
				builderManager->EnqueueTask(CBuilderTask::Priority::NORMAL, circuit->GetUnitDefByName("corrl"), pos, CBuilderTask::TaskType::DEFENDER);
				return task;
			}
		}
	}

	return task;
}

CBuilderTask* CEconomyManager::UpdateEnergyTasks(const AIFloat3& position)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	CBuilderTask* task = nullptr;
	if (!builderManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* solarDef = circuit->GetUnitDefByName("armsolar");
	UnitDef* fusDef = circuit->GetUnitDefByName("armfus");
	UnitDef* singuDef = circuit->GetUnitDefByName("cafus");
	UnitDef* pylonDef = circuit->GetUnitDefByName("armestor");

	// check energy / metal ratio
	float energyIncome = eco->GetIncome(energyRes);
	float metalIncome = eco->GetIncome(metalRes);
	float energyUsage = eco->GetUsage(energyRes);

	// Solar task
	if (((metalIncome > energyIncome * 0.5) || (energyUsage > energyIncome * 0.5)) && (solarCount < 10) && circuit->IsAvailable(solarDef)) {
		float cost = solarDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 4 + 2;
		if (builderManager->GetTasks(CBuilderTask::TaskType::SOLAR).size() < count) {
			CMetalManager* metalManager = circuit->GetMetalManager();
			int index = metalManager->FindNearestSpot(position);
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
			task = builderManager->EnqueueTask(CBuilderTask::Priority::HIGH, solarDef, buildPos, CBuilderTask::TaskType::SOLAR, cost);
		}
		return task;
	}

	// Fusion task
	if ((energyUsage > energyIncome * 0.6) && (solarCount >= 10) && (fusionCount < 3) && circuit->IsAvailable(fusDef)) {
		float cost = fusDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 4 + 1;
		if (builderManager->GetTasks(CBuilderTask::TaskType::FUSION).size() < count) {
			const AIFloat3& startPos = circuit->GetSetupManager()->GetStartPos();
			CMetalManager* metalManager = circuit->GetMetalManager();
			int index = metalManager->FindNearestCluster(startPos);
			AIFloat3 buildPos;
			if (index >= 0) {
				const std::vector<AIFloat3>& centroids = metalManager->GetCentroids();
				buildPos = centroids[index];
			} else {
				CTerrainManager* terrain = circuit->GetTerrainManager();
				int terWidth = terrain->GetTerrainWidth();
				int terHeight = terrain->GetTerrainHeight();
				float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
				float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
				buildPos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
			}
			task = builderManager->EnqueueTask(CBuilderTask::Priority::NORMAL, fusDef, buildPos, CBuilderTask::TaskType::FUSION);
		}
		return task;
	}

	// Singularity task
	if ((energyUsage > energyIncome * 0.7) && (fusionCount >= 3) && circuit->IsAvailable(singuDef)) {
		float cost = singuDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 4 + 1;
		if (builderManager->GetTasks(CBuilderTask::TaskType::SINGU).size() < count) {
			const AIFloat3& startPos = circuit->GetSetupManager()->GetStartPos();
			CMetalManager* metalManager = circuit->GetMetalManager();
			CMetalData::MetalIndices indices = metalManager->FindNearestClusters(startPos, 3);
			AIFloat3 buildPos;
			if (!indices.empty()) {
				const std::vector<AIFloat3>& centroids = metalManager->GetCentroids();
				int index = indices[rand() % indices.size()];
				buildPos = centroids[index];
			} else {
				CTerrainManager* terrain = circuit->GetTerrainManager();
				int terWidth = terrain->GetTerrainWidth();
				int terHeight = terrain->GetTerrainHeight();
				float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
				float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
				buildPos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
			}
			task = builderManager->EnqueueTask(CBuilderTask::Priority::NORMAL, singuDef, buildPos, CBuilderTask::TaskType::SINGU);
		}
		return task;
	}

	return task;
}

CBuilderTask* CEconomyManager::UpdateBuilderTasks(const AIFloat3& position)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	CBuilderTask* task = nullptr;
	if (!builderManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* assistDef = circuit->GetUnitDefByName("armnanotc");
	UnitDef* facDef = circuit->GetUnitDefByName("factorycloak");

	// check buildpower
	float metalIncome = eco->GetIncome(metalRes);
	CFactoryManager* factoryManager = circuit->GetFactoryManager();
	if ((factoryManager->GetFactoryPower() < metalIncome) &&
			builderManager->GetTasks(CBuilderTask::TaskType::FACTORY).empty() && builderManager->GetTasks(CBuilderTask::TaskType::NANO).empty()) {
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
			task = builderManager->EnqueueTask(CBuilderTask::Priority::NORMAL, assistDef, buildPos, CBuilderTask::TaskType::NANO);
		} else if (circuit->IsAvailable(facDef)) {
			CMetalData::MetalPredicate predicate = [this](const CMetalData::MetalNode& v) {
				return clusterInfos[v.second].factory == nullptr;
			};
			CMetalManager* metalManager = circuit->GetMetalManager();
			metalManager->ClusterLock();
			int index = metalManager->FindNearestCluster(position, predicate);
			CTerrainManager* terrain = circuit->GetTerrainManager();
			AIFloat3 buildPos;
			if (index >= 0) {
				const std::vector<AIFloat3>& centroids = metalManager->GetCentroids();
				buildPos = centroids[index];
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
			metalManager->ClusterUnlock();
			task = builderManager->EnqueueTask(CBuilderTask::Priority::HIGH, facDef, buildPos, CBuilderTask::TaskType::FACTORY);
		}
	}

	return task;
}

CFactoryTask* CEconomyManager::UpdateFactoryTasks()
{
	CFactoryManager* factoryManager = circuit->GetFactoryManager();
	CFactoryTask* task = nullptr;
	if (!factoryManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* buildDef = circuit->GetUnitDefByName("armrectr");

	float metalIncome = eco->GetIncome(metalRes);
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if ((builderManager->GetBuilderPower() < metalIncome * 1.8) && circuit->IsAvailable(buildDef)) {
		for (auto t : factoryManager->GetTasks()) {
			if (t->GetType() == CFactoryTask::TaskType::BUILDPOWER) {
				return task;
			}
		}
		CCircuitUnit* factory = factoryManager->GetRandomFactory();
		const AIFloat3& buildPos = factory->GetUnit()->GetPos();
		CTerrainManager* terrain = circuit->GetTerrainManager();
		float radius = std::max(terrain->GetTerrainWidth(), terrain->GetTerrainHeight()) / 4;
		task = factoryManager->EnqueueTask(CFactoryTask::Priority::NORMAL, buildDef, buildPos, CFactoryTask::TaskType::BUILDPOWER, 1, radius);
	}

	return task;
}

CBuilderTask* CEconomyManager::UpdateStorageTasks()
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	CBuilderTask* task = nullptr;
	if (!builderManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* storeDef = circuit->GetUnitDefByName("armmstor");
	UnitDef* pylonDef = circuit->GetUnitDefByName("armestor");

	float income = eco->GetIncome(metalRes);
	float storage = eco->GetStorage(metalRes);
	if (builderManager->GetTasks(CBuilderTask::TaskType::STORE).empty() && (storage / income < 25) && circuit->IsAvailable(storeDef)) {
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
		task = builderManager->EnqueueTask(CBuilderTask::Priority::HIGH, storeDef, buildPos, CBuilderTask::TaskType::STORE);
		return task;
	}

	if ((fusionCount > 1) && (pylonCount < pylonMaxCount) && circuit->IsAvailable(pylonDef)) {
		float cost = pylonDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 2 + 1;
		if (builderManager->GetTasks(CBuilderTask::TaskType::PYLON).size() < count) {
//			CMetalData::MetalPredicate predicate = [this](const CMetalData::MetalNode& v) {
//				return clusterInfos[v.second].pylon == nullptr;
//			};
			CMetalManager* metalManager = circuit->GetMetalManager();
			const AIFloat3& startPos = circuit->GetSetupManager()->GetStartPos();
			metalManager->ClusterLock();
			int index = metalManager->FindNearestCluster(startPos/*, predicate*/);
			if (index >= 0) {
				const std::vector<AIFloat3>& centroids = metalManager->GetCentroids();
				task = builderManager->EnqueueTask(CBuilderTask::Priority::LOW, pylonDef, centroids[index], CBuilderTask::TaskType::PYLON);
			}
			metalManager->ClusterUnlock();
			return task;
		}
	}

	return task;
}

} // namespace circuit
