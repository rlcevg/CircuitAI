/*
 * EconomyManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "module/EconomyManager.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "setup/SetupManager.h"
#include "resource/MetalManager.h"
#include "resource/EnergyGrid.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/math/LagrangeInterPol.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "Map.h"
#include "SkirmishAIs.h"
#include "Resource.h"
#include "Economy.h"
#include "Feature.h"
#include "FeatureDef.h"

namespace circuit {

using namespace springai;

#define INCOME_SAMPLES	5

CEconomyManager::CEconomyManager(CCircuitAI* circuit) :
		IModule(circuit),
		indexRes(0),
		metalIncome(.0f),
		energyIncome(.0f)
{
	metalRes = circuit->GetCallback()->GetResourceByName("Metal");
	energyRes = circuit->GetCallback()->GetResourceByName("Energy");
	eco = circuit->GetCallback()->GetEconomy();

	metalIncomes.resize(INCOME_SAMPLES, .0f);
	energyIncomes.resize(INCOME_SAMPLES, .0f);

	pylonDef = circuit->GetCircuitDef("armestor");
	UnitDef* def = pylonDef->GetUnitDef();
	const std::map<std::string, std::string>& customParams = def->GetCustomParams();
	auto search = customParams.find("pylonrange");
	pylonRange = (search != customParams.end()) ? utils::string_to_float(search->second) : PYLON_RANGE;

	// TODO: Use A* ai planning... or sth... STRIPS https://ru.wikipedia.org/wiki/STRIPS
	//       https://ru.wikipedia.org/wiki/Марковский_процесс_принятия_решений

	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateResourceIncome, this), TEAM_SLOWUPDATE_RATE);
	scheduler->RunParallelTask(CGameTask::emptyTask, std::make_shared<CGameTask>(&CEconomyManager::Init, this));

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
	unitDefId = circuit->GetCircuitDef("factorycloak")->GetId();
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

	// FIXME: Cost thresholds/ecoFactor should rely on alive allies
	ecoFactor = circuit->GetAllyTeam()->GetSize() * 0.25f + 0.75f;

	/*
	 *  Identify resource buildings
	 */
	CCircuitAI::CircuitDefs& allDefs = circuit->GetCircuitDefs();
	for (auto& kv : allDefs) {
		CCircuitDef* cdef = kv.second;
		UnitDef* def = cdef->GetUnitDef();
		if (def->GetSpeed() <= 0) {
			const std::map<std::string, std::string>& customParams = def->GetCustomParams();
			auto it = customParams.find("income_energy");
			if ((it != customParams.end()) && (utils::string_to_float(it->second) > 1)) {
				// TODO: Filter only defs that we are able to build
				allEnergyDefs.insert(cdef);
			} else if (((it = customParams.find("ismex")) != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
				mexDef = cdef;  // cormex
			}
		}
	}

	// TODO: Make configurable
	// Using cafus, armfus, armsolar as control points
	// FIXME: Дабы ветка параболы заработала надо использовать [x <= 0; y < min limit) для точки перегиба
	const char* engies[] = {"cafus", "armfus", "armsolar"};
	const int limits[] = {5, 3, 8};  // TODO: range randomize
	const int size = sizeof(engies) / sizeof(engies[0]);
	CLagrangeInterPol::Vector x(size), y(size);
	for (int i = 0; i < size; ++i) {
		UnitDef* def = circuit->GetCircuitDef(engies[i])->GetUnitDef();
		float make = utils::string_to_float(def->GetCustomParams().find("income_energy")->second);
		x[i] = def->GetCost(metalRes) / make;
		y[i] = limits[i] + 0.5;  // +0.5 to be sure precision errors will not decrease integer part
	}
	engyPol = new CLagrangeInterPol(x, y);  // Alternatively use CGaussSolver to compute polynomial - faster on reuse
}

CEconomyManager::~CEconomyManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete metalRes, energyRes, eco;
	delete engyPol;
}

int CEconomyManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetCircuitDef()->GetId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitFinished(CCircuitUnit* unit)
{
	auto search = finishedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

IBuilderTask* CEconomyManager::CreateBuilderTask(CCircuitUnit* unit)
{
	// TODO: Add general logic here
	const AIFloat3& pos = unit->GetUnit()->GetPos();

	IBuilderTask* task;
	task = UpdateMetalTasks(pos, unit);
	if (task != nullptr) {
		return task;
	}
	task = UpdateEnergyTasks(pos, unit);
	if (task != nullptr) {
		return task;
	}
	task = UpdateFactoryTasks(pos, unit);
	if (task != nullptr) {
		return task;
	}

	// FIXME: Eco rules. It should never get here
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome()) * ecoFactor;
	CCircuitDef* buildDef = circuit->GetCircuitDef("armwin");
	if ((metalIncome < 50) && (buildDef->GetCount() < 10)) {
		task = builderManager->EnqueueTask(IBuilderTask::Priority::LOW, buildDef, pos, IBuilderTask::BuildType::ENERGY);
	} else if (metalIncome < 100) {
		task = builderManager->EnqueuePatrol(IBuilderTask::Priority::LOW, pos, .0f, FRAMES_PER_SEC * 20);
	} else {
		const std::set<IBuilderTask*>& tasks = builderManager->GetTasks(IBuilderTask::BuildType::BIG_GUN);
		if (tasks.empty()) {
			buildDef = circuit->GetCircuitDef("striderhub");
			if (buildDef->GetCount() < 1) {
				CMetalData::MetalPredicate predicate = [this](const CMetalData::MetalNode& v) {
					return clusterInfos[v.second].factory == nullptr;
				};
				CMetalManager* metalManager = circuit->GetMetalManager();
				int index = metalManager->FindNearestCluster(circuit->GetSetupManager()->GetBasePos(), predicate);
				CTerrainManager* terrain = circuit->GetTerrainManager();
				AIFloat3 buildPos;
				if (index >= 0) {
					buildPos = metalManager->GetClusters()[index].geoCentr;
					float size = 10 * SQUARE_SIZE;
					buildPos.x += (buildPos.x > terrain->GetTerrainWidth() / 2) ? size : -size;
					buildPos.z += (buildPos.z > terrain->GetTerrainHeight() / 2) ? size : -size;
					if (terrain->CanBeBuiltAt(buildDef, buildPos) && terrain->CanBuildAt(unit, buildPos)) {
						task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, buildDef, buildPos, IBuilderTask::BuildType::BIG_GUN);
					} else {
						task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, buildDef, circuit->GetSetupManager()->GetBasePos(), IBuilderTask::BuildType::BIG_GUN);
					}
				} else {
					task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, buildDef, circuit->GetSetupManager()->GetBasePos(), IBuilderTask::BuildType::BIG_GUN);
				}
			} else {
				buildDef = circuit->GetCircuitDef("raveparty");
				if (buildDef->GetCount() < 1) {
					task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, buildDef, circuit->GetSetupManager()->GetBasePos(), IBuilderTask::BuildType::BIG_GUN);
				} else {
					task = builderManager->EnqueuePatrol(IBuilderTask::Priority::LOW, pos, .0f, FRAMES_PER_SEC * 20);
				}
			}
		} else {
			task = *tasks.begin();
		}
	}
	return task;  // Must not return nullptr
}

CRecruitTask* CEconomyManager::CreateFactoryTask(CCircuitUnit* unit)
{
	// TODO: Add general logic here
	CRecruitTask* task = UpdateRecruitTasks();
	if (task != nullptr) {
		return task;
	}
//	return (CRecruitTask*)circuit->GetFactoryManager()->GetIdleTask();

	const char* names3[] = {"armrock", "armpw", "armwar", "armsnipe", "armjeth", "armzeus"};
	const char* names2[] = {"armpw", "armrock", "armpw", "armwar", "armsnipe", "armzeus"};
	const char* names1[] = {"armpw", "armrock", "armpw", "armwar", "armpw", "armrock"};
	char** names;
	float metalIncome = GetAvgMetalIncome() * ecoFactor;
	if (metalIncome > 30) {
		names = (char**)names3;
	} else if (metalIncome > 20) {
		names = (char**)names2;
	} else {
		names = (char**)names1;
	}
	CCircuitDef* buildDef = circuit->GetCircuitDef(names[rand() % 6]);
	const AIFloat3& buildPos = unit->GetUnit()->GetPos();
	UnitDef* def = unit->GetCircuitDef()->GetUnitDef();
	float radius = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE * 4;
	task = circuit->GetFactoryManager()->EnqueueTask(CRecruitTask::Priority::LOW, buildDef, buildPos, CRecruitTask::BuildType::DEFAULT, radius);
	return task;
}

IBuilderTask* CEconomyManager::CreateAssistTask(CCircuitUnit* unit)
{
	CCircuitUnit* repairTarget = nullptr;
	CCircuitUnit* buildTarget = nullptr;
	bool isBuildMobile = true;
	const AIFloat3& pos = unit->GetUnit()->GetPos();
	float radius = unit->GetCircuitDef()->GetBuildDistance();
	float sqRadius = radius * radius;

	/*
	 * Check for damaged units
	 */
	float maxCost = MAX_BUILD_SEC * GetAvgMetalIncome() * ecoFactor;
	CCircuitDef* terraDef = circuit->GetBuilderManager()->GetTerraDef();
	circuit->UpdateFriendlyUnits();
	auto units = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius));
	for (auto u : units) {
		CCircuitUnit* candUnit = circuit->GetFriendlyUnit(u);
		if (candUnit == nullptr) {
			continue;
		}
		if (u->IsBeingBuilt()) {
			if ((pos.SqDistance2D(u->GetPos()) < sqRadius)) {
				CCircuitDef* cdef = candUnit->GetCircuitDef();
				if (isBuildMobile && ((cdef->GetUnitDef()->GetCost(metalRes) < maxCost) || (*cdef == *terraDef))) {
					isBuildMobile = candUnit->GetUnit()->GetMaxSpeed() > 0;
					buildTarget = candUnit;
				}
			}
		} else if (u->GetHealth() < u->GetMaxHealth() && (pos.SqDistance2D(u->GetPos()) < sqRadius)) {
			repairTarget = candUnit;
			break;
		}
	}
	utils::free_clear(units);
	if (repairTarget != nullptr) {
		// Repair task
		return circuit->GetFactoryManager()->EnqueueRepair(IBuilderTask::Priority::LOW, repairTarget);
	}

	/*
	 * Check metal storage and unit under construction
	 */
	if (IsMetalEmpty()) {
		// Reclaim task
		auto features = std::move(circuit->GetCallback()->GetFeaturesIn(pos, radius));
		bool valid = !features.empty();
		utils::free_clear(features);
		if (valid) {
			return circuit->GetFactoryManager()->EnqueueReclaim(IBuilderTask::Priority::LOW, pos, radius);
		}
	}
	if (buildTarget != nullptr) {
		// Construction task
		return circuit->GetFactoryManager()->EnqueueRepair(IBuilderTask::Priority::LOW, buildTarget);
	}

	return nullptr;
}

Resource* CEconomyManager::GetMetalRes() const
{
	return metalRes;
}

Resource* CEconomyManager::GetEnergyRes() const
{
	return energyRes;
}

CCircuitDef* CEconomyManager::GetMexDef() const
{
	return mexDef;
}

CCircuitDef* CEconomyManager::GetPylonDef() const
{
	return pylonDef;
}

float CEconomyManager::GetPylonRange() const
{
	return pylonRange;
}

void CEconomyManager::AddEnergyDefs(const std::set<CCircuitDef*>& buildDefs)
{
	// TODO: Cache engyDefs in CCircuitDef?
	std::set<CCircuitDef*> engyDefs;
	std::set_intersection(allEnergyDefs.begin(), allEnergyDefs.end(),
						  buildDefs.begin(), buildDefs.end(),
						  std::inserter(engyDefs, engyDefs.begin()));
	std::set<CCircuitDef*> diffDefs;
	std::set_difference(engyDefs.begin(), engyDefs.end(),
						availEnergyDefs.begin(), availEnergyDefs.end(),
						std::inserter(diffDefs, diffDefs.begin()));
	if (diffDefs.empty()) {
		return;
	}
	availEnergyDefs.insert(diffDefs.begin(), diffDefs.end());

	for (auto cdef : diffDefs) {
		UnitDef* def = cdef->GetUnitDef();
		SEnergyInfo engy;
		engy.cdef = cdef;
		engy.cost = def->GetCost(metalRes);
		engy.costDivMake = engy.cost / utils::string_to_float(def->GetCustomParams().find("income_energy")->second);
		engy.limit = engyPol->GetValueAt(engy.costDivMake);
		energyInfos.push_back(engy);
	}

	// High-tech energy first
	auto compare = [](const SEnergyInfo& e1, const SEnergyInfo& e2) {
		return e1.costDivMake < e2.costDivMake;
	};
	energyInfos.sort(compare);
}

void CEconomyManager::RemoveEnergyDefs(const std::set<CCircuitDef*>& buildDefs)
{
	// TODO: Cache engyDefs in CCircuitDef?
	std::set<CCircuitDef*> engyDefs;
	std::set_intersection(allEnergyDefs.begin(), allEnergyDefs.end(),
						  buildDefs.begin(), buildDefs.end(),
						  std::inserter(engyDefs, engyDefs.begin()));
	std::set<CCircuitDef*> diffDefs;
	std::set_difference(availEnergyDefs.begin(), availEnergyDefs.end(),
						engyDefs.begin(), engyDefs.end(),
						std::inserter(diffDefs, diffDefs.begin()));
	if (diffDefs.empty()) {
		return;
	}
	availEnergyDefs.erase(diffDefs.begin(), diffDefs.end());

	auto it = energyInfos.begin();
	while (it != energyInfos.end()) {
		auto search = diffDefs.find(it->cdef);
		if (search != diffDefs.end()) {
			it = energyInfos.erase(it);
		}
	}
}

void CEconomyManager::UpdateResourceIncome()
{
	energyIncomes[indexRes] = eco->GetIncome(energyRes);
	metalIncomes[indexRes] = eco->GetIncome(metalRes) + eco->GetReceived(metalRes);
	indexRes++;
	indexRes %= INCOME_SAMPLES;

	metalIncome = .0f;
	for (int i = 0; i < INCOME_SAMPLES; i++) {
		metalIncome += metalIncomes[i];
	}
	metalIncome /= INCOME_SAMPLES;

	energyIncome = .0f;
	for (int i = 0; i < INCOME_SAMPLES; i++) {
		energyIncome += energyIncomes[i];
	}
	energyIncome /= INCOME_SAMPLES;
}

float CEconomyManager::GetAvgMetalIncome() const
{
	return metalIncome;
}

float CEconomyManager::GetAvgEnergyIncome() const
{
	return energyIncome;
}

float CEconomyManager::GetEcoFactor() const
{
	return ecoFactor;
}

bool CEconomyManager::IsMetalFull() const
{
	return eco->GetCurrent(metalRes) > eco->GetStorage(metalRes) * 0.8;
}

bool CEconomyManager::IsMetalEmpty() const
{
	return eco->GetCurrent(metalRes) < eco->GetStorage(metalRes) * 0.2;
}

bool CEconomyManager::IsEnergyStalling() const
{
	return GetAvgMetalIncome() > GetAvgEnergyIncome() * 0.8;
}

IBuilderTask* CEconomyManager::UpdateMetalTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask()) {
		return nullptr;
	}
	IBuilderTask* task = nullptr;

	// check uncolonized mexes
	int taskSize = builderManager->GetTasks(IBuilderTask::BuildType::MEX).size();
	bool isEnergyStalling = IsEnergyStalling();
	bool mustHave = !isEnergyStalling || ((taskSize < 1) && (builderManager->GetWorkerCount() > 2));
	if (mustHave && mexDef->IsAvailable()) {
		UnitDef* metalDef =  mexDef->GetUnitDef();
		float cost = metalDef->GetCost(metalRes);
		int maxCount = builderManager->GetBuilderPower() / cost * 4 + 2;
		if (taskSize < maxCount) {
			CMetalManager* metalManager = circuit->GetMetalManager();
			const CMetalData::Metals& spots = metalManager->GetSpots();
			Map* map = circuit->GetMap();
			CMetalData::MetalPredicate predicate;
			if (unit != nullptr) {
				CTerrainManager* terrainManager = circuit->GetTerrainManager();
				predicate = [&spots, metalManager, map, metalDef, terrainManager, unit](CMetalData::MetalNode const& v) {
					int index = v.second;
					return (metalManager->IsOpenSpot(index) &&
							terrainManager->CanBuildAt(unit, spots[index].position) &&
							map->IsPossibleToBuildAt(metalDef, spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
				};
			} else {
				CCircuitDef* mexDef = this->mexDef;
				predicate = [&spots, metalManager, map, mexDef, builderManager](CMetalData::MetalNode const& v) {
					int index = v.second;
					return (metalManager->IsOpenSpot(index) &&
							builderManager->IsBuilderInArea(mexDef, spots[index].position) &&
							map->IsPossibleToBuildAt(mexDef->GetUnitDef(), spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
				};
			}
			int index = metalManager->FindNearestSpot(position, predicate);
			if (index != -1) {
				const AIFloat3& pos = spots[index].position;
				task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, mexDef, pos, IBuilderTask::BuildType::MEX, cost);
				task->SetBuildPos(pos);
				metalManager->SetOpenSpot(index, false);
				return task;
			}
		}
	}

	if (!isEnergyStalling) {
		task = UpdateReclaimTasks(position, unit);
	}

	return task;
}

IBuilderTask* CEconomyManager::UpdateReclaimTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask() || (unit == nullptr)) {
		return nullptr;
	}
	IBuilderTask* task = nullptr;

	if (IsMetalFull() || builderManager->GetTasks(IBuilderTask::BuildType::RECLAIM).size() >= builderManager->GetWorkerCount() / 2) {
		return nullptr;
	}
	auto features = std::move(circuit->GetCallback()->GetFeaturesIn(position, unit->GetUnit()->GetMaxSpeed() * FRAMES_PER_SEC * 60));
	if (!features.empty()) {
		CTerrainManager* terrain = circuit->GetTerrainManager();
		AIFloat3 reclPos;
		float minSqDist = std::numeric_limits<float>::max();
		for (Feature* feature : features) {
			AIFloat3 featPos = feature->GetPosition();
			terrain->CorrectPosition(featPos);  // Impulsed flying feature
			if (!terrain->CanBuildAt(unit, featPos)) {
				continue;
			}
			FeatureDef* featDef = feature->GetDef();
			float reclaimValue = featDef->GetContainedResource(metalRes)/* * feature->GetReclaimLeft()*/;
			delete featDef;
			if (reclaimValue < 1.0f) {
				continue;
			}
			float sqDist = position.SqDistance2D(featPos);
			if (sqDist < minSqDist) {
				reclPos = featPos;
				minSqDist = sqDist;
			}
		}
		if (minSqDist < std::numeric_limits<float>::max()) {
			task = builderManager->EnqueueReclaim(IBuilderTask::Priority::LOW, reclPos, 1.0f, FRAMES_PER_SEC * 300, unit->GetCircuitDef()->GetBuildDistance());
		}
	}
	utils::free_clear(features);

	return task;
}

IBuilderTask* CEconomyManager::UpdateEnergyTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask()) {
		return nullptr;
	}

	// check energy / metal ratio
	float energyIncome = GetAvgEnergyIncome();
	bool isEnergyStalling = IsEnergyStalling();
	if (!isEnergyStalling && (eco->GetUsage(energyRes) <= energyIncome * 0.8)) {
		return nullptr;
	}

	// Select proper energy UnitDef to build
	CCircuitDef* bestDef = nullptr;
	float cost;
	float metalIncome = std::min(GetAvgMetalIncome(), energyIncome);
	float buildPower = std::min(builderManager->GetBuilderPower(), metalIncome * 0.5f);
	int taskSize = builderManager->GetTasks(IBuilderTask::BuildType::ENERGY).size();
	float maxBuildTime = MAX_BUILD_SEC * (isEnergyStalling ? 0.1f : ecoFactor);
	for (auto& engy : energyInfos) {  // sorted by high-tech first
		// TODO: Add geothermal powerplant support
		if (!engy.cdef->IsAvailable() || engy.cdef->GetUnitDef()->IsNeedGeo()) {
			continue;
		}

		if (engy.cdef->GetCount() < engy.limit) {
			int maxCount = buildPower / engy.cost * 4 + 1;
			if (taskSize < maxCount) {
				cost = engy.cost;
				bestDef = engy.cdef;
				// TODO: Select proper scale/quadratic function (x*x) and smoothing coefficient (8).
				//       МЕТОД НАИМЕНЬШИХ КВАДРАТОВ ! (income|buildPower, make/cost) - points
				//       solar       geothermal    fusion         singu           ...
				//       (10, 2/70), (15, 25/500), (20, 35/1000), (30, 225/4000), ...
				if (cost / (buildPower * buildPower / 8) < maxBuildTime) {
					break;
				}
			}
		} else {
			break;
		}
	}

	if (bestDef != nullptr) {
		// Find place to build
		AIFloat3 buildPos = -RgtVector;

		CMetalManager* metalManager = circuit->GetMetalManager();
		if (cost / std::min(builderManager->GetBuilderPower(), metalIncome) < MIN_BUILD_SEC) {
			int index = metalManager->FindNearestSpot(position);
			if (index != -1) {
				const CMetalData::Metals& spots = metalManager->GetSpots();
				buildPos = spots[index].position;
			}
		} else {
			const AIFloat3& startPos = circuit->GetSetupManager()->GetBasePos();
			int index = metalManager->FindNearestCluster(startPos);
			if (index >= 0) {
				const CMetalData::Clusters& clusters = metalManager->GetClusters();
				buildPos = clusters[index].geoCentr;
			}
		}

		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		if ((buildPos != -RgtVector) && terrainManager->CanBeBuiltAt(bestDef, buildPos) &&
			((unit == nullptr) || terrainManager->CanBuildAt(unit, buildPos)))
		{
			IBuilderTask::Priority priority = isEnergyStalling ? IBuilderTask::Priority::HIGH : IBuilderTask::Priority::NORMAL;
			return builderManager->EnqueueTask(priority, bestDef, buildPos, IBuilderTask::BuildType::ENERGY, cost);
		}
	}

	return nullptr;
}

IBuilderTask* CEconomyManager::UpdateFactoryTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask()) {
		return nullptr;
	}

	CFactoryManager* factoryManager = circuit->GetFactoryManager();

	// check buildpower
	float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome());
	if ((factoryManager->GetFactoryPower() >= metalIncome) ||
		!builderManager->GetTasks(IBuilderTask::BuildType::FACTORY).empty() ||
		!builderManager->GetTasks(IBuilderTask::BuildType::NANO).empty())
	{
		return nullptr;
	}

	CCircuitDef* assistDef = factoryManager->GetAssistDef();
	CCircuitUnit* factory = factoryManager->NeedUpgrade();
	if ((factory != nullptr) && assistDef->IsAvailable()) {
		Unit* u = factory->GetUnit();
		UnitDef* def = factory->GetCircuitDef()->GetUnitDef();
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

		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		if (terrainManager->CanBeBuiltAt(assistDef, buildPos) &&
			((unit == nullptr) || terrainManager->CanBuildAt(unit, buildPos)))
		{
			return builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, assistDef, buildPos, IBuilderTask::BuildType::NANO);
		}
	}

	CCircuitDef* facDef = circuit->GetCircuitDef("factorycloak");
	if (facDef->IsAvailable()) {
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
			UnitDef* facUDef = facDef->GetUnitDef();
			float size = std::max(facUDef->GetXSize(), facUDef->GetZSize()) * SQUARE_SIZE;
			buildPos.x += (buildPos.x > terrain->GetTerrainWidth() / 2) ? -size : size;
			buildPos.z += (buildPos.z > terrain->GetTerrainHeight() / 2) ? -size : size;

			if (terrain->CanBeBuiltAt(facDef, buildPos) && ((unit == nullptr) || terrain->CanBuildAt(unit, buildPos)))
			{
				return builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, facDef, buildPos, IBuilderTask::BuildType::FACTORY);
			}
		}
	}

	return nullptr;
}

CRecruitTask* CEconomyManager::UpdateRecruitTasks()
{
	CFactoryManager* factoryManager = circuit->GetFactoryManager();
	if (!factoryManager->CanEnqueueTask()) {
		return nullptr;
	}

	CCircuitDef* buildDef = circuit->GetCircuitDef("armrectr");

	float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome());
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	// TODO: Create ReclaimTask for 20% of workers, and 20% RepairTask.
	if ((builderManager->GetBuilderPower() < metalIncome * 2.0f) && (rand() % 2 == 0) && buildDef->IsAvailable()) {
		CCircuitUnit* factory = factoryManager->GetRandomFactory();
		const AIFloat3& buildPos = factory->GetUnit()->GetPos();
		CTerrainManager* terrain = circuit->GetTerrainManager();
		float radius = std::max(terrain->GetTerrainWidth(), terrain->GetTerrainHeight()) / 4;
		return factoryManager->EnqueueTask(CRecruitTask::Priority::NORMAL, buildDef, buildPos, CRecruitTask::BuildType::BUILDPOWER, radius);
	}

	return nullptr;
}

IBuilderTask* CEconomyManager::UpdateStorageTasks()
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask()) {
		return nullptr;
	}

	CCircuitDef* storeDef = circuit->GetCircuitDef("armmstor");

	float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome());
	if (!builderManager->GetTasks(IBuilderTask::BuildType::STORE).empty() || (eco->GetStorage(metalRes) > 10 * metalIncome) || !storeDef->IsAvailable()) {
		return UpdatePylonTasks();
	}

	const AIFloat3& startPos = circuit->GetSetupManager()->GetBasePos();
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
	return builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, storeDef, buildPos, IBuilderTask::BuildType::STORE);
}

IBuilderTask* CEconomyManager::UpdatePylonTasks()
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask()) {
		return nullptr;
	}

	float energyIncome = GetAvgEnergyIncome();
	float metalIncome = std::min(GetAvgMetalIncome(), energyIncome);
	if ((metalIncome * ecoFactor < 10) || (energyIncome < 80) || !pylonDef->IsAvailable()) {
		return nullptr;
	}

	float cost = pylonDef->GetUnitDef()->GetCost(metalRes);
	int count = builderManager->GetBuilderPower() / cost * 8 + 1;
	if (builderManager->GetTasks(IBuilderTask::BuildType::PYLON).size() < count) {
		CEnergyGrid* grid = circuit->GetEnergyGrid();
		grid->Update();

		CCircuitDef* buildDef;
		AIFloat3 buildPos;
		CEnergyLink* link = grid->GetLinkToBuild(buildDef, buildPos);
		if (link != nullptr) {
			if ((buildPos != -RgtVector) && builderManager->IsBuilderInArea(buildDef, buildPos)) {
				return builderManager->EnqueuePylon(IBuilderTask::Priority::HIGH, buildDef, buildPos, link, cost);
			} else {
				link->SetValid(false);  // FIXME: Reset valid on timer? Or when air con appears
				grid->SetForceRebuild(true);
			}
		}
	}

	return nullptr;
}

void CEconomyManager::Init()
{
	const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
	clusterInfos.resize(clusters.size());

	for (int k = 0; k < clusters.size(); ++k) {
		clusterInfos[k] = {nullptr};
	}

	SkirmishAIs* ais = circuit->GetCallback()->GetSkirmishAIs();
	const int interval = ais->GetSize() * FRAMES_PER_SEC;
	delete ais;
	const AIFloat3& pos = circuit->GetSetupManager()->GetBasePos();
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateFactoryTasks, this, pos, nullptr), interval, circuit->GetSkirmishAIId() + 0 + 10 * interval);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateStorageTasks, this), interval, circuit->GetSkirmishAIId() + 1);
}

} // namespace circuit
