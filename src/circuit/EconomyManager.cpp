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
#include "BuilderManager.h"
#include "FactoryManager.h"
#include "utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Map.h"
#include "SkirmishAIs.h"
#include "Resource.h"
#include "Economy.h"
//#include "WeaponDef.h"
#include "Pathing.h"
#include "MoveData.h"

//#include "Drawer.h"

namespace circuit {

using namespace springai;

CEconomyManager::CEconomyManager(CCircuitAI* circuit, CBuilderManager* builderManager, CFactoryManager* factoryManager) :
		IModule(circuit),
		builderManager(builderManager),
		factoryManager(factoryManager),
		solarCount(0),
		fusionCount(0)
{
	metalRes = circuit->GetCallback()->GetResourceByName("Metal");
	energyRes = circuit->GetCallback()->GetResourceByName("Energy");
	eco = circuit->GetCallback()->GetEconomy();

	UnitDef* def = circuit->GetUnitDefByName("armestor");
	std::map<std::string, std::string> customParams = def->GetCustomParams();
	pylonRange = utils::string_to_float(customParams["pylonrange"]);

//	WeaponDef* wpDef = circuit->GetCallback()->GetWeaponDefByName("nuclear_missile");
//	singuRange = wpDef->GetAreaOfEffect();
//	delete wpDef;

	// TODO: Use A* ai planning... or sth... STRIPS https://ru.wikipedia.org/wiki/STRIPS
	//       https://ru.wikipedia.org/wiki/Марковский_процесс_принятия_решений

	SkirmishAIs* ais = circuit->GetCallback()->GetSkirmishAIs();
	int aisCount = ais->GetSize();
	delete ais;
	// FIXME: Remove parallel clusterization (and Init). Task is fast enough for main process and too much issues with parallelism.
	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>(&CEconomyManager::Init, this));
	const int interval = aisCount * 4;
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateExpandTasks, this), interval, circuit->GetSkirmishAIId() + 0);
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateEnergyTasks, this), interval, circuit->GetSkirmishAIId() + 1);
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateBuilderTasks, this), interval, circuit->GetSkirmishAIId() + 2 + 100 * interval);
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateFactoryTasks, this), interval, circuit->GetSkirmishAIId() + 3);

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
		int index = this->circuit->GetGameAttribute()->GetMetalManager().FindNearestCluster(unit->GetUnit()->GetPos());
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
		int index = this->circuit->GetGameAttribute()->GetMetalManager().FindNearestCluster(unit->GetUnit()->GetPos());
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

	builderManager->SetEconomyManager(this);
	factoryManager->SetEconomyManager(this);
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
	CBuilderTask* task;
	task = UpdateExpandTasks();
	if (task != nullptr) {
		return task;
	}
	task = UpdateEnergyTasks();
	if (task != nullptr) {
		return task;
	}
	task = UpdateBuilderTasks();
	if (task != nullptr) {
		return task;
	}
	const AIFloat3& pos = unit->GetUnit()->GetPos();
	UnitDef* buildDef = circuit->GetUnitDefByName("corrl");
	task = builderManager->EnqueueTask(CBuilderTask::Priority::LOW, buildDef, pos, CBuilderTask::TaskType::DEFAULT);
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
	CFactoryTask* task = factoryManager->EnqueueTask(CFactoryTask::Priority::LOW, buildDef, buildPos, CFactoryTask::TaskType::DEFAULT, 2, radius);
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
			const CMetalManager::Metals& spots = circuit->GetGameAttribute()->GetMetalManager().GetSpots();
			Map* map = circuit->GetMap();
			CMetalManager::MetalPredicate predicate = [&spots, map, buildDef](CMetalManager::MetalNode const& v) {
				return map->IsPossibleToBuildAt(buildDef, spots[v.second].position, UNIT_COMMAND_BUILD_NO_FACING);
			};
			int index = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpot(position, predicate);
			if (index >= 0) {
				const CMetalManager::Metals& spots =  circuit->GetGameAttribute()->GetMetalManager().GetSpots();
				buildPos = spots[index].position;
			}
			break;
		}
		case CBuilderTask::TaskType::PYLON: {
			const AIFloat3& position = task->GetPos();
//			buildPos = circuit->GetMap()->FindClosestBuildSite(buildDef, position, pylonRange, 0, UNIT_COMMAND_BUILD_NO_FACING);
			buildPos = circuit->FindBuildSite(buildDef, position, pylonRange, UNIT_COMMAND_BUILD_NO_FACING);
			if (buildPos == -RgtVector) {
				const CMetalManager::Metals& spots =  circuit->GetGameAttribute()->GetMetalManager().GetSpots();
				CMetalManager::MetalPredicate predicate = [this](const CMetalManager::MetalNode& v) {
					return clusterInfo[v.second].pylon == nullptr;
				};
				CMetalManager::MetalIndices indices = circuit->GetGameAttribute()->GetMetalManager().FindNearestClusters(position, 3, predicate);
				for (const int idx : indices) {
					buildPos = circuit->FindBuildSite(buildDef, spots[idx].position, pylonRange, UNIT_COMMAND_BUILD_NO_FACING);
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
	const std::vector<CMetalManager::MetalIndices>& clusters = circuit->GetGameAttribute()->GetMetalManager().GetClusters();
	clusterInfo.resize(clusters.size());
	for (int i = 0; i < clusters.size(); i++) {
		clusterInfo[i] = {nullptr};
	}
}

CBuilderTask* CEconomyManager::UpdateExpandTasks()
{
	CBuilderTask* task = nullptr;
	if (!builderManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* buildDef = circuit->GetUnitDefByName("cormex");

	// check uncolonized mexes
	if (builderManager->GetTasks(CBuilderTask::TaskType::EXPAND).empty() && (buildDef->GetMaxThisUnit() > 0)) {
		const AIFloat3& startPos = circuit->GetStartPos();
		const CMetalManager::Metals& spots = circuit->GetGameAttribute()->GetMetalManager().GetSpots();
		Map* map = circuit->GetMap();
		CMetalManager::MetalPredicate predicate = [&spots, map, buildDef](CMetalManager::MetalNode const& v) {
			return map->IsPossibleToBuildAt(buildDef, spots[v.second].position, UNIT_COMMAND_BUILD_NO_FACING);
		};
		float cost = buildDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 4 + 1;
		CMetalManager::MetalIndices indices = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpots(startPos, count, predicate);
		for (auto idx : indices) {
			CBuilderTask* task = builderManager->EnqueueTask(CBuilderTask::Priority::LOW, buildDef, spots[idx].position, CBuilderTask::TaskType::EXPAND, cost);
			task->SetBuildPos(spots[idx].position);
		}
	}

	return task;
}

CBuilderTask* CEconomyManager::UpdateEnergyTasks()
{
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
	if ((energyUsage > energyIncome * 1.1) && (solarCount < 16) && builderManager->GetTasks(CBuilderTask::TaskType::SOLAR).empty() && (solarDef->GetMaxThisUnit() > 0)) {
		const AIFloat3& startPos = circuit->GetStartPos();
		float cost = solarDef->GetCost(metalRes);
		int count = builderManager->GetBuilderPower() / cost * 4 + 2;
		CMetalManager::MetalIndices indices = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpots(startPos, count);
		if (!indices.empty()) {
			const CMetalManager::Metals& spots = circuit->GetGameAttribute()->GetMetalManager().GetSpots();
			for (auto idx : indices) {
				task = builderManager->EnqueueTask(CBuilderTask::Priority::HIGH, solarDef, spots[idx].position, CBuilderTask::TaskType::SOLAR, cost);
			}
		} else {
			Map* map = circuit->GetMap();
			int terWidth = circuit->GetTerrainWidth();
			int terHeight = circuit->GetTerrainHeight();
			const int numSolars = 2;
			for (int i = 0; i < numSolars; i++) {
				float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
				float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
				AIFloat3 buildPos = AIFloat3(x, map->GetElevationAt(x, z), z);
				task = builderManager->EnqueueTask(CBuilderTask::Priority::HIGH, solarDef, buildPos, CBuilderTask::TaskType::SOLAR, cost);
			}
		}
	}
	else if ((energyUsage > energyIncome * 1.1) && (solarCount >= 16) && (fusionCount < 5) && builderManager->GetTasks(CBuilderTask::TaskType::FUSION).empty() && (fusDef->GetMaxThisUnit() > 0)) {
		const AIFloat3& startPos = circuit->GetStartPos();
		int index = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpot(startPos);
		if (index >= 0) {
			const CMetalManager::Metals& spots = circuit->GetGameAttribute()->GetMetalManager().GetSpots();
			task = builderManager->EnqueueTask(CBuilderTask::Priority::LOW, fusDef, spots[index].position, CBuilderTask::TaskType::FUSION);
		} else {
			int terWidth = circuit->GetTerrainWidth();
			int terHeight = circuit->GetTerrainHeight();
			float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
			float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
			AIFloat3 buildPos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
			task = builderManager->EnqueueTask(CBuilderTask::Priority::LOW, fusDef, buildPos, CBuilderTask::TaskType::FUSION);
		}
		if (pylonDef->GetMaxThisUnit() > 0) {
			CMetalManager::MetalPredicate predicate = [this](const CMetalManager::MetalNode& v) {
				return clusterInfo[v.second].pylon == nullptr;
			};
			index = circuit->GetGameAttribute()->GetMetalManager().FindNearestCluster(startPos, predicate);
			if (index >= 0) {
				const std::vector<AIFloat3>& centroids = circuit->GetGameAttribute()->GetMetalManager().GetCentroids();
				builderManager->EnqueueTask(CBuilderTask::Priority::LOW, pylonDef, centroids[index], CBuilderTask::TaskType::PYLON);
			}
		}
	}
	else if ((fusionCount >= 5) && builderManager->GetTasks(CBuilderTask::TaskType::SINGU).empty() && (singuDef->GetMaxThisUnit() > 0)) {
		const AIFloat3& startPos = circuit->GetStartPos();
		CMetalManager::MetalIndices indices = circuit->GetGameAttribute()->GetMetalManager().FindNearestSpots(startPos, 3);
		if (!indices.empty()) {
			const CMetalManager::Metals& spots = circuit->GetGameAttribute()->GetMetalManager().GetSpots();
			int index = indices[rand() % indices.size()];
			task = builderManager->EnqueueTask(CBuilderTask::Priority::LOW, singuDef, spots[index].position, CBuilderTask::TaskType::SINGU);
		} else {
			int terWidth = circuit->GetTerrainWidth();
			int terHeight = circuit->GetTerrainHeight();
			float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
			float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
			AIFloat3 buildPos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
			task = builderManager->EnqueueTask(CBuilderTask::Priority::LOW, singuDef, buildPos, CBuilderTask::TaskType::SINGU);
		}
		if (pylonDef->GetMaxThisUnit() > 0) {
			CMetalManager::MetalPredicate predicate = [this](const CMetalManager::MetalNode& v) {
				return clusterInfo[v.second].pylon == nullptr;
			};
			int index = circuit->GetGameAttribute()->GetMetalManager().FindNearestCluster(startPos, predicate);
			if (index >= 0) {
				const std::vector<AIFloat3>& centroids = circuit->GetGameAttribute()->GetMetalManager().GetCentroids();
				builderManager->EnqueueTask(CBuilderTask::Priority::LOW, pylonDef, centroids[index], CBuilderTask::TaskType::PYLON);
			}
		}
	}

	return task;
}

CBuilderTask* CEconomyManager::UpdateBuilderTasks()
{
	CBuilderTask* task = nullptr;
	if (!builderManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* assistDef = circuit->GetUnitDefByName("armnanotc");
	UnitDef* facDef = circuit->GetUnitDefByName("factorycloak");

	// check buildpower
	float metalIncome = eco->GetIncome(metalRes);
	if ((factoryManager->GetFactoryPower() < metalIncome) &&
			builderManager->GetTasks(CBuilderTask::TaskType::FACTORY).empty() && builderManager->GetTasks(CBuilderTask::TaskType::NANO).empty()) {
		CCircuitUnit* factory = factoryManager->NeedUpgrade();
		if ((factory != nullptr) && (assistDef->GetMaxThisUnit() > 0)) {
			Unit* u = factory->GetUnit();
			UnitDef* def = factory->GetDef();
			AIFloat3 buildPos = u->GetPos();
			switch (u->GetBuildingFacing()) {
				default:
				case UNIT_FACING_SOUTH:
					buildPos.z -= def->GetZSize() * 0.55 * SQUARE_SIZE;
					break;
				case UNIT_FACING_EAST:
					buildPos.x -= def->GetXSize() * 0.55 * SQUARE_SIZE;
					break;
				case UNIT_FACING_NORTH:
					buildPos.z += def->GetZSize() * 0.55 * SQUARE_SIZE;
					break;
				case UNIT_FACING_WEST:
					buildPos.x += def->GetXSize() * 0.55 * SQUARE_SIZE;
					break;
			}
			task = builderManager->EnqueueTask(CBuilderTask::Priority::LOW, assistDef, buildPos, CBuilderTask::TaskType::NANO);
		} else if (facDef->GetMaxThisUnit() > 0) {
			const AIFloat3& startPos = circuit->GetStartPos();
			CMetalManager::MetalPredicate predicate = [this](const CMetalManager::MetalNode& v) {
				return clusterInfo[v.second].factory == nullptr;
			};
			int index = circuit->GetGameAttribute()->GetMetalManager().FindNearestCluster(startPos, predicate);
			AIFloat3 buildPos;
			if (index >= 0) {
				const std::vector<AIFloat3>& centroids = circuit->GetGameAttribute()->GetMetalManager().GetCentroids();
				buildPos = centroids[index];
			} else {
				int terWidth = circuit->GetTerrainWidth();
				int terHeight = circuit->GetTerrainHeight();
				float x = terWidth/4 + rand() % (int)(terWidth/2 + 1);
				float z = terHeight/4 + rand() % (int)(terHeight/2 + 1);
				buildPos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
			}
			task = builderManager->EnqueueTask(CBuilderTask::Priority::LOW, facDef, buildPos, CBuilderTask::TaskType::FACTORY);
		}
	}

	return task;
}

CFactoryTask* CEconomyManager::UpdateFactoryTasks()
{
	CFactoryTask* task = nullptr;
	if (!factoryManager->CanEnqueueTask()) {
		return task;
	}

	UnitDef* buildDef = circuit->GetUnitDefByName("armrectr");

	float metalIncome = eco->GetIncome(metalRes);
//	printf("metalIncome: %.2f, totalBuildpower: %.2f, factoryPower: %.2f\n", metalIncome, totalBuildpower, factoryPower);
	if ((builderManager->GetBuilderPower() < metalIncome * 1.5) && (buildDef->GetMaxThisUnit() > 0)) {
		for (auto t : factoryManager->GetTasks()) {
			if (t->GetType() == CFactoryTask::TaskType::BUILDPOWER) {
				return task;
			}
		}
		CCircuitUnit* factory = factoryManager->GetRandomFactory();
		const AIFloat3& buildPos = factory->GetUnit()->GetPos();
		float radius = std::max(circuit->GetTerrainWidth(), circuit->GetTerrainHeight()) / 4;
		task = factoryManager->EnqueueTask(CFactoryTask::Priority::NORMAL, buildDef, buildPos, CFactoryTask::TaskType::BUILDPOWER, 2, radius);
	}

	return task;
}

} // namespace circuit
