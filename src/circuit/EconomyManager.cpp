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

namespace circuit {

using namespace springai;

CEconomyManager::CEconomyManager(CCircuitAI* circuit) :
		IModule(circuit),
		solarCount(0),
		fusionCount(0)
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
			clusterInfo[index].factory = unit;
		}
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		for (auto& info : clusterInfo) {
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
		this->solarCount++;
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		this->solarCount--;
	};

	/*
	 * armfus handlers
	 */
	unitDefId = circuit->GetUnitDefByName("armfus")->GetUnitDefId();
	createdHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		this->fusionCount++;
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		this->fusionCount--;
	};

	/*
	 * armestor handlers
	 */
	unitDefId = circuit->GetUnitDefByName("armestor")->GetUnitDefId();
	createdHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		// check pylon's cluster
		int index = this->circuit->GetMetalManager()->FindNearestCluster(unit->GetUnit()->GetPos());
		if (index >= 0) {
			clusterInfo[index].pylon = unit;
		}
	};
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CCircuitUnit* attacker) {
		for (auto& info : clusterInfo) {
			if (info.pylon == unit) {
				info.pylon = nullptr;
			}
		}
	};
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
	const AIFloat3& pos = unit->GetUnit()->GetPos();

	CBuilderTask* task;
	task = UpdateEnergyTasks(pos);
	if (task != nullptr) {
		return task;
	}
	task = UpdateMetalTasks(pos);
	if (task != nullptr) {
		return task;
	}
	task = UpdateBuilderTasks(pos);
	if (task != nullptr) {
		return task;
	}

	UnitDef* buildDef = circuit->GetUnitDefByName("corrl");
	task = circuit->GetBuilderManager()->EnqueueTask(CBuilderTask::Priority::LOW, buildDef, pos, CBuilderTask::TaskType::DEFAULT);
	return task;
}

CFactoryTask* CEconomyManager::CreateFactoryTask(CCircuitUnit* unit)
{
	// TODO: Add general logic here
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
	CFactoryTask* task = circuit->GetFactoryManager()->EnqueueTask(CFactoryTask::Priority::LOW, buildDef, buildPos, CFactoryTask::TaskType::DEFAULT, 1, radius);
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
	switch (task->GetType()) {
		case CBuilderTask::TaskType::EXPAND: {
			const AIFloat3& position = u->GetPos();
			const CMetalData::Metals& spots = circuit->GetMetalManager()->GetSpots();
			Map* map = circuit->GetMap();
			CMetalData::MetalPredicate predicate = [&spots, map, buildDef](CMetalData::MetalNode const& v) {
				return map->IsPossibleToBuildAt(buildDef, spots[v.second].position, UNIT_COMMAND_BUILD_NO_FACING);
			};
			int index = circuit->GetMetalManager()->FindNearestSpot(position, predicate);
			if (index >= 0) {
				const CMetalData::Metals& spots =  circuit->GetMetalManager()->GetSpots();
				buildPos = spots[index].position;
			}
			break;
		}
		case CBuilderTask::TaskType::PYLON: {
			const AIFloat3& position = task->GetPos();
			CTerrainManager* terrain = circuit->GetTerrainManager();
			buildPos = terrain->FindBuildSite(buildDef, position, pylonRange, UNIT_COMMAND_BUILD_NO_FACING);
			if (buildPos == -RgtVector) {
				const CMetalData::Metals& spots =  circuit->GetMetalManager()->GetSpots();
				CMetalData::MetalPredicate predicate = [this](const CMetalData::MetalNode& v) {
					return clusterInfo[v.second].pylon == nullptr;
				};
				CMetalData::MetalIndices indices = circuit->GetMetalManager()->FindNearestClusters(position, 3, predicate);
				for (const int idx : indices) {
					buildPos = terrain->FindBuildSite(buildDef, spots[idx].position, pylonRange, UNIT_COMMAND_BUILD_NO_FACING);
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
	clusterInfo.resize(clusters.size());
	for (int i = 0; i < clusters.size(); i++) {
		clusterInfo[i] = {nullptr};
	}

	SkirmishAIs* ais = circuit->GetCallback()->GetSkirmishAIs();
	const int interval = ais->GetSize() * 4;
	delete ais;
	const AIFloat3& pos = circuit->GetSetupManager()->GetStartPos();
//	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateMetalTasks, this, pos), interval, circuit->GetSkirmishAIId() + 0);
//	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateEnergyTasks, this, pos), interval, circuit->GetSkirmishAIId() + 1);
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateBuilderTasks, this, pos), interval, circuit->GetSkirmishAIId() + 2 + 100 * interval);
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateFactoryTasks, this), interval, circuit->GetSkirmishAIId() + 3);
}

CBuilderTask* CEconomyManager::UpdateMetalTasks(const AIFloat3& position)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	CBuilderTask* task = nullptr;
	if (!builderManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* metalDef = circuit->GetUnitDefByName("cormex");
	UnitDef* storeDef = circuit->GetUnitDefByName("armmstor");

	// check uncolonized mexes
	float cost = metalDef->GetCost(metalRes);
	int count = builderManager->GetBuilderPower() / cost * 4 + 1;
	if ((builderManager->GetTasks(CBuilderTask::TaskType::EXPAND).size() < count) && circuit->IsAvailable(metalDef)) {
		const CMetalData::Metals& spots = circuit->GetMetalManager()->GetSpots();
		Map* map = circuit->GetMap();
		CMetalData::MetalPredicate predicate = [&spots, map, metalDef](CMetalData::MetalNode const& v) {
			// TODO: Ignore spots with task on it
			return map->IsPossibleToBuildAt(metalDef, spots[v.second].position, UNIT_COMMAND_BUILD_NO_FACING);
		};
		int index = circuit->GetMetalManager()->FindNearestSpot(position, predicate);
		if (index != -1) {
			task = builderManager->EnqueueTask(CBuilderTask::Priority::HIGH, metalDef, spots[index].position, CBuilderTask::TaskType::EXPAND, cost);
			task->SetBuildPos(spots[index].position);
			return task;
		}
	}

	float income = eco->GetIncome(metalRes);
	float storage = eco->GetStorage(metalRes);
	if (builderManager->GetTasks(CBuilderTask::TaskType::STORE).empty() && (storage / income < 25) && circuit->IsAvailable(storeDef)) {
		const AIFloat3& startPos = circuit->GetSetupManager()->GetStartPos();
		int index = circuit->GetMetalManager()->FindNearestSpot(startPos);
		AIFloat3 buildPos;
		if (index != -1) {
			const CMetalData::Metals& spots = circuit->GetMetalManager()->GetSpots();
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
	float energyUsage = eco->GetUsage(energyRes);

	// Solar task
	if ((energyUsage > energyIncome) && (solarCount < 10) && circuit->IsAvailable(solarDef)) {
		float cost = solarDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 2 + 1;
		if ((builderManager->GetTasks(CBuilderTask::TaskType::SOLAR).size() < count)) {
			int index = circuit->GetMetalManager()->FindNearestSpot(position);
			AIFloat3 buildPos;
			if (index != -1) {
				const CMetalData::Metals& spots = circuit->GetMetalManager()->GetSpots();
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
	if ((energyUsage > energyIncome) && (solarCount >= 10) && (fusionCount < 5) && circuit->IsAvailable(fusDef)) {
		float cost = fusDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 2 + 1;
		if (builderManager->GetTasks(CBuilderTask::TaskType::FUSION).size() < count) {
			int index = circuit->GetMetalManager()->FindNearestCluster(position);
			AIFloat3 buildPos;
			if (index >= 0) {
				const std::vector<AIFloat3>& centroids = circuit->GetMetalManager()->GetCentroids();
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
			if (circuit->IsAvailable(pylonDef)) {
				CMetalData::MetalPredicate predicate = [this](const CMetalData::MetalNode& v) {
					return clusterInfo[v.second].pylon == nullptr;
				};
				CMetalManager* metalManager = circuit->GetMetalManager();
				metalManager->ClusterLock();
				index = metalManager->FindNearestCluster(position, predicate);
				if (index >= 0) {
					const std::vector<AIFloat3>& centroids = metalManager->GetCentroids();
					builderManager->EnqueueTask(CBuilderTask::Priority::NORMAL, pylonDef, centroids[index], CBuilderTask::TaskType::PYLON);
				}
				metalManager->ClusterUnlock();
			}
		}
		return task;
	}

	// Singularity task
	if ((energyUsage > energyIncome) && (fusionCount >= 5) && circuit->IsAvailable(singuDef)) {
		float cost = singuDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 2 + 1;
		if (builderManager->GetTasks(CBuilderTask::TaskType::SINGU).size() < count) {
			const AIFloat3& startPos = circuit->GetSetupManager()->GetStartPos();
			CMetalData::MetalIndices indices = circuit->GetMetalManager()->FindNearestClusters(startPos, 3);
			AIFloat3 buildPos;
			if (!indices.empty()) {
				const std::vector<AIFloat3>& centroids = circuit->GetMetalManager()->GetCentroids();
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
			if (circuit->IsAvailable(pylonDef)) {
				CMetalData::MetalPredicate predicate = [this](const CMetalData::MetalNode& v) {
					return clusterInfo[v.second].pylon == nullptr;
				};
				CMetalManager* metalManager = circuit->GetMetalManager();
				metalManager->ClusterLock();
				int index = metalManager->FindNearestCluster(startPos, predicate);
				if (index >= 0) {
					const std::vector<AIFloat3>& centroids = metalManager->GetCentroids();
					builderManager->EnqueueTask(CBuilderTask::Priority::NORMAL, pylonDef, centroids[index], CBuilderTask::TaskType::PYLON);
				}
				metalManager->ClusterUnlock();
			}
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
			task = builderManager->EnqueueTask(CBuilderTask::Priority::HIGH, assistDef, buildPos, CBuilderTask::TaskType::NANO);
		} else if (circuit->IsAvailable(facDef)) {
			CMetalData::MetalPredicate predicate = [this](const CMetalData::MetalNode& v) {
				return clusterInfo[v.second].factory == nullptr;
			};
			CMetalManager* metalManager = circuit->GetMetalManager();
			metalManager->ClusterLock();
			int index = metalManager->FindNearestCluster(position, predicate);
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
	if ((builderManager->GetBuilderPower() < metalIncome * 1.5) && circuit->IsAvailable(buildDef)) {
		for (auto t : factoryManager->GetTasks()) {
			if (t->GetType() == CFactoryTask::TaskType::BUILDPOWER) {
				return task;
			}
		}
		CCircuitUnit* factory = factoryManager->GetRandomFactory();
		const AIFloat3& buildPos = factory->GetUnit()->GetPos();
		CTerrainManager* terrain = circuit->GetTerrainManager();
		float radius = std::max(terrain->GetTerrainWidth(), terrain->GetTerrainHeight()) / 4;
		task = factoryManager->EnqueueTask(CFactoryTask::Priority::NORMAL, buildDef, buildPos, CFactoryTask::TaskType::BUILDPOWER, 2, radius);
	}

	return task;
}

} // namespace circuit
