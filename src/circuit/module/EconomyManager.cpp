/*
 * EconomyManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "module/EconomyManager.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "resource/MetalManager.h"
#include "resource/EnergyGrid.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/math/LagrangeInterPol.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#include "json/json.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "Map.h"
#include "SkirmishAIs.h"
#include "Resource.h"
#include "Economy.h"
#include "Feature.h"
#include "FeatureDef.h"
#include "Team.h"
#include "TeamRulesParam.h"
#include "UnitRulesParam.h"

namespace circuit {

using namespace springai;

#define INCOME_SAMPLES	5
#define HIDDEN_ENERGY	10000.0f

CEconomyManager::CEconomyManager(CCircuitAI* circuit)
		: IModule(circuit)
		, indexRes(0)
		, metalIncome(.0f)
		, energyIncome(.0f)
		, empParam(nullptr)
		, eepParam(nullptr)
		, odeiParam(nullptr)
		, odecParam(nullptr)
//		, odeoParam(nullptr)
//		, odteParam(nullptr)
//		, odaParam(nullptr)
		, ecoFrame(-1)
		, isMetalEmpty(false)
		, isMetalFull(false)
		, isEnergyStalling(false)
		, isEnergyEmpty(false)
		, metalPullFrame(-1)
		, energyPullFrame(-1)
		, metalPull(.0f)
		, energyPull(.0f)
{
	metalRes = circuit->GetCallback()->GetResourceByName("Metal");
	energyRes = circuit->GetCallback()->GetResourceByName("Energy");
	economy = circuit->GetCallback()->GetEconomy();
	energyGrid = circuit->GetAllyTeam()->GetEnergyLink().get();

	metalIncomes.resize(INCOME_SAMPLES, 4.0f);  // Init metal income
	energyIncomes.resize(INCOME_SAMPLES, 6.0f);  // Init energy income

	// TODO: Use A* ai planning... or sth... STRIPS https://ru.wikipedia.org/wiki/STRIPS
	//       https://ru.wikipedia.org/wiki/Марковский_процесс_принятия_решений

	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateResourceIncome, this), TEAM_SLOWUPDATE_RATE);
	scheduler->RunTaskAt(std::make_shared<CGameTask>(&CEconomyManager::Init, this));

	/*
	 * factory handlers
	 */
	auto factoryFinishedHandler = [this](CCircuitUnit* unit) {
		int frame = this->circuit->GetLastFrame();
		int index = this->circuit->GetMetalManager()->FindNearestCluster(unit->GetPos(frame));
		if (index >= 0) {
			clusterInfos[index].factory = unit;
		}
	};
	auto factoryDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
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
	 * resources
	 */
	auto energyFinishedHandler = [this](CCircuitUnit* unit) {
		auto it = std::find(energyInfos.begin(), energyInfos.end(), unit->GetCircuitDef());
		if (it != energyInfos.end()) {
			float income = it->cost / it->costDivMake;
			for (int i = 0; i < INCOME_SAMPLES; ++i) {
				energyIncomes[i] += income;
			}
			energyIncome += income;
		}
	};
	auto mexFinishedHandler = [this](CCircuitUnit* unit) {
		UnitRulesParam* p = unit->GetUnit()->GetUnitRulesParamByName("mexIncome");
		if (p != nullptr) {
			float income = p->GetValueFloat();
			for (int i = 0; i < INCOME_SAMPLES; ++i) {
				metalIncomes[i] += income;
			}
			metalIncome += income;
			delete p;
		}
	};

	/*
	 * morphing
	 */
	auto comFinishedHandler = [this](CCircuitUnit* unit) {
		AddMorphee(unit);
		this->circuit->GetSetupManager()->SetCommander(unit);
	};
	auto comDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		RemoveMorphee(unit);
	};

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CCircuitDef* pylonCandy = nullptr;
	float maxAreaDivCost = .0f;

	const CCircuitAI::CircuitDefs& allDefs = circuit->GetCircuitDefs();
	for (auto& kv : allDefs) {
		CCircuitDef* cdef = kv.second;
		const std::map<std::string, std::string>& customParams = cdef->GetUnitDef()->GetCustomParams();

		if (!cdef->IsMobile()) {
			// pylon
			auto it = customParams.find("pylonrange");
			if (it != customParams.end()) {
				float areaDivCost = M_PI * SQUARE(utils::string_to_float(it->second)) / cdef->GetCost();
				if (maxAreaDivCost < areaDivCost) {
					maxAreaDivCost = areaDivCost;
					pylonCandy = cdef;  // armestor
				}
			}

			if (cdef->GetUnitDef()->IsBuilder() && !cdef->GetBuildOptions().empty()) {
				// factory
				finishedHandler[cdef->GetId()] = factoryFinishedHandler;
				destroyedHandler[cdef->GetId()] = factoryDestroyedHandler;
			}

			it = customParams.find("income_energy");
			if ((it != customParams.end()) && (utils::string_to_float(it->second) > 1)) {
				// energy
				finishedHandler[kv.first] = energyFinishedHandler;
				if (!cdef->IsAvailable() || ((terrainManager->GetPercentLand() < 40.0) &&
					(terrainManager->GetImmobileTypeById(cdef->GetImmobileId())->minElevation >= .0f)))
				{
					continue;
				}
				allEnergyDefs.insert(cdef);
			} else if (((it = customParams.find("ismex")) != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
				// mex
				finishedHandler[kv.first] = mexFinishedHandler;
				mexDef = cdef;  // cormex
			}

		} else {

			// commander
			auto it = customParams.find("level");
			if ((it != customParams.end()) && (utils::string_to_int(it->second) <= 6)) {  // TODO: Identify max level
				// NOTE: last level is needed only to set proper commander
				finishedHandler[cdef->GetId()] = comFinishedHandler;
				destroyedHandler[cdef->GetId()] = comDestroyedHandler;
			}
		}
	}

	pylonDef = pylonCandy;
	UnitDef* def = pylonDef->GetUnitDef();
	const std::map<std::string, std::string>& customParams = def->GetCustomParams();
	auto search = customParams.find("pylonrange");
	pylonRange = (search != customParams.end()) ? utils::string_to_float(search->second) : PYLON_RANGE;

	ReadConfig();
}

CEconomyManager::~CEconomyManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete metalRes;
	delete energyRes;
	delete economy;
	delete empParam;
	delete eepParam;
	delete odeiParam;
	delete odecParam/*, delete odeoParam, delete odteParam, delete odaParam*/;
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

int CEconomyManager::UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	// NOTE: If more actions should be done then consider moving into damagedHandler
	if (unit->IsMorphing() && (unit->GetUnit()->GetHealth() < unit->GetUnit()->GetMaxHealth() * 0.5f)) {
		unit->StopUpgrade();  // StopMorph();
		AddMorphee(unit);
	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

CCircuitDef* CEconomyManager::GetLowEnergy(const AIFloat3& pos) const
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CCircuitDef* candidate = nullptr;
	auto it = energyInfos.rbegin();
	while (it != energyInfos.rend()) {
		if (terrainManager->CanBeBuiltAt(it->cdef, pos)) {
			candidate = it->cdef;
			break;
		}
		++it;
	}
	return candidate;
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
		SEnergyInfo engy;
		engy.cdef = cdef;
		engy.cost = cdef->GetCost();
		float make = utils::string_to_float(cdef->GetUnitDef()->GetCustomParams().find("income_energy")->second);
		engy.costDivMake = engy.cost / make;
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
	if (odeiParam == nullptr) {
		odeiParam = circuit->GetTeam()->GetTeamRulesParamByName("OD_energyIncome");
	}
	float oddEnergyIncome = (odeiParam != nullptr) ? odeiParam->GetValueFloat() : .0f;
	if (odecParam == nullptr) {
		odecParam = circuit->GetTeam()->GetTeamRulesParamByName("OD_energyChange");
	}
	float oddEnergyChange = (odecParam != nullptr) ? odecParam->GetValueFloat() : .0f;

	energyIncomes[indexRes] = economy->GetIncome(energyRes) + oddEnergyIncome - std::max(.0f, oddEnergyChange);
	metalIncomes[indexRes] = economy->GetIncome(metalRes) + economy->GetReceived(metalRes);
	++indexRes %= INCOME_SAMPLES;

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

float CEconomyManager::GetMetalPull()
{
	if (metalPullFrame/* + TEAM_SLOWUPDATE_RATE*/ < circuit->GetLastFrame()) {
		metalPullFrame = circuit->GetLastFrame();
		if (empParam == nullptr) {
			empParam = circuit->GetTeam()->GetTeamRulesParamByName("extraMetalPull");
		}
		metalPull = economy->GetPull(metalRes) + (empParam != nullptr ? empParam->GetValueFloat() : .0f);
	}
	return metalPull;
}

float CEconomyManager::GetEnergyPull()
{
	if (energyPullFrame/* + TEAM_SLOWUPDATE_RATE*/ < circuit->GetLastFrame()) {
		energyPullFrame = circuit->GetLastFrame();
		if (eepParam == nullptr) {
			eepParam = circuit->GetTeam()->GetTeamRulesParamByName("extraEnergyPull");
		}
		float extraEnergyPull = (eepParam != nullptr) ? eepParam->GetValueFloat() : .0f;
//		if (odeoParam == nullptr) {
//			odeoParam = circuit->GetTeam()->GetTeamRulesParamByName("OD_energyOverdrive");
//		}
//		float oddEnergyOverdrive = (odeoParam != nullptr) ? odeoParam->GetValueFloat() : .0f;
//		if (odecParam == nullptr) {
//			odecParam = circuit->GetTeam()->GetTeamRulesParamByName("OD_energyChange");
//		}
//		float oddEnergyChange = (odecParam != nullptr) ? odecParam->GetValueFloat() : .0f;
//		float extraChange = std::min(.0f, oddEnergyChange) - std::min(.0f, oddEnergyOverdrive);
//		if (odteParam == nullptr) {
//			odteParam = circuit->GetTeam()->GetTeamRulesParamByName("OD_team_energyWaste");
//		}
//		float teamEnergyWaste = (odteParam != nullptr) ? odteParam->GetValueFloat() : .0f;
//		if (odaParam == nullptr) {
//			odaParam = circuit->GetTeam()->GetTeamRulesParamByName("OD_allies");
//		}
//		float numAllies = (odaParam != nullptr) ? odaParam->GetValueFloat() : 1.0f;
		energyPull = economy->GetPull(energyRes) + extraEnergyPull/* + extraChange - teamEnergyWaste / numAllies*/;
	}
	return energyPull;
}

bool CEconomyManager::IsMetalEmpty()
{
	UpdateEconomy();
	return isMetalEmpty;
}

bool CEconomyManager::IsMetalFull()
{
	UpdateEconomy();
	return isMetalFull;
}

bool CEconomyManager::IsEnergyStalling()
{
	UpdateEconomy();
	return isEnergyStalling;
}

bool CEconomyManager::IsEnergyEmpty()
{
	UpdateEconomy();
	return isEnergyEmpty;
}

IBuilderTask* CEconomyManager::UpdateMetalTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask()) {
		return nullptr;
	}
	IBuilderTask* task = nullptr;

	// check uncolonized mexes
	bool isEnergyStalling = IsEnergyStalling();
	if (!isEnergyStalling && mexDef->IsAvailable()) {
		float cost = mexDef->GetCost();
		unsigned maxCount = builderManager->GetBuilderPower() / cost * 8 + 1;
		if (builderManager->GetTasks(IBuilderTask::BuildType::MEX).size() < maxCount) {
			CMetalManager* metalManager = circuit->GetMetalManager();
			const CMetalData::Metals& spots = metalManager->GetSpots();
			Map* map = circuit->GetMap();
			CMetalManager::MexPredicate predicate;
			if (unit != nullptr) {
				UnitDef* metalDef =  mexDef->GetUnitDef();
				CTerrainManager* terrainManager = circuit->GetTerrainManager();
				predicate = [&spots, metalManager, map, metalDef, terrainManager, unit](int index) {
					return (metalManager->IsOpenSpot(index) &&
							terrainManager->CanBuildAt(unit, spots[index].position) &&
							map->IsPossibleToBuildAt(metalDef, spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
				};
			} else {
				CCircuitDef* mexDef = this->mexDef;
				predicate = [&spots, metalManager, map, mexDef, builderManager](int index) {
					return (metalManager->IsOpenSpot(index) &&
							builderManager->IsBuilderInArea(mexDef, spots[index].position) &&
							map->IsPossibleToBuildAt(mexDef->GetUnitDef(), spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
				};
			}
			int index = metalManager->GetMexToBuild(position, predicate);
			if (index != -1) {
				int cluster = metalManager->GetCluster(index);
				if (!circuit->GetMilitaryManager()->HasDefence(cluster)) {
					circuit->GetMilitaryManager()->AddDefendTask(cluster);
				}

				const AIFloat3& pos = spots[index].position;
				task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, mexDef, pos, IBuilderTask::BuildType::MEX, cost, .0f);
				task->SetBuildPos(pos);
				metalManager->SetOpenSpot(index, false);
				return task;
			}
		}
	}

	task = isEnergyStalling ? UpdateEnergyTasks(position, unit) : UpdateReclaimTasks(position, unit);

	return task;
}

IBuilderTask* CEconomyManager::UpdateReclaimTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (/*!builderManager->CanEnqueueTask() || */(unit == nullptr)) {
		return nullptr;
	}
	IBuilderTask* task = nullptr;

	if (IsMetalFull() || (builderManager->GetTasks(IBuilderTask::BuildType::RECLAIM).size() >= builderManager->GetWorkerCount() / 2)) {
		return nullptr;
	}
	float travelDistance = unit->GetUnit()->GetMaxSpeed() * FRAMES_PER_SEC * ((GetMetalPull() * 0.8f > GetAvgMetalIncome()) ? 300 : 30);
	auto features = std::move(circuit->GetCallback()->GetFeaturesIn(position, travelDistance));
	if (!features.empty()) {
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		AIFloat3 pos;
		float cost = .0f;
		float minSqDist = std::numeric_limits<float>::max();
		for (Feature* feature : features) {
			AIFloat3 featPos = feature->GetPosition();
			terrainManager->CorrectPosition(featPos);  // Impulsed flying feature
			if (!terrainManager->CanBuildAt(unit, featPos)) {
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
				pos = featPos;
				cost = reclaimValue;
				minSqDist = sqDist;
			}
		}
		if (minSqDist < std::numeric_limits<float>::max()) {
			IBuilderTask* task = nullptr;
			for (IBuilderTask* t : builderManager->GetTasks(IBuilderTask::BuildType::RECLAIM)) {
				if (utils::is_equal_pos(pos, t->GetTaskPos())) {
					task = t;
					break;
				}
			}
			if (task == nullptr) {
				task = builderManager->EnqueueReclaim(IBuilderTask::Priority::NORMAL, pos, cost, FRAMES_PER_SEC * 300,
													  8.0f/*unit->GetCircuitDef()->GetBuildDistance()*/);
			}
		}
		utils::free_clear(features);
	}

	return task;
}

IBuilderTask* CEconomyManager::UpdateEnergyTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask()) {
		return nullptr;
	}

	// check energy / metal ratio
	float metalIncome = GetAvgMetalIncome();
	float energyIncome = GetAvgEnergyIncome();
	bool isEnergyStalling = IsEnergyStalling();

	// Select proper energy UnitDef to build
	CCircuitDef* bestDef = nullptr;
	float cost;
	metalIncome = std::min(metalIncome, energyIncome) * incomeFactor;
	float buildPower = std::min(builderManager->GetBuilderPower(), metalIncome);
	int taskSize = builderManager->GetTasks(IBuilderTask::BuildType::ENERGY).size();
	float maxBuildTime = MAX_BUILD_SEC * (isEnergyStalling ? 0.8f : ecoFactor);
	for (const SEnergyInfo& engy : energyInfos) {  // sorted by high-tech first
		// TODO: Add geothermal powerplant support
		if (!engy.cdef->IsAvailable() || engy.cdef->GetUnitDef()->IsNeedGeo()) {
			continue;
		}

		if (engy.cdef->GetCount() < engy.limit) {
			int maxCount = buildPower / engy.cost * 16 + 1;
			if (taskSize < maxCount) {
				cost = engy.cost;
				bestDef = engy.cdef;
				// TODO: Select proper scale/quadratic function (x*x) and smoothing coefficient (8).
				//       МЕТОД НАИМЕНЬШИХ КВАДРАТОВ ! (income|buildPower, make/cost) - points
				//       solar       geothermal    fusion         singu           ...
				//       (10, 2/70), (15, 25/500), (20, 35/1000), (30, 225/4000), ...
				if (cost * 16.0f < maxBuildTime * SQUARE(buildPower)) {
					break;
				}
			}
		} else if (!isEnergyStalling) {
			bestDef = nullptr;
			break;
		}
	}

	if (bestDef != nullptr) {
		// Find place to build
		AIFloat3 buildPos = -RgtVector;

		CMetalManager* metalManager = circuit->GetMetalManager();
		if (cost < MIN_BUILD_SEC * buildPower) {
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

				// TODO: Calc enemy vector and move position into opposite direction
				AIFloat3 mapCenter(circuit->GetTerrainManager()->GetTerrainWidth() / 2, 0, circuit->GetTerrainManager()->GetTerrainHeight() / 2);
				buildPos += (buildPos - mapCenter).Normalize2D() * 300.0f * (cost / energyInfos.front().cost);
				CCircuitDef* bdef = (unit == nullptr) ? bestDef : unit->GetCircuitDef();
				buildPos = circuit->GetTerrainManager()->GetBuildPosition(bdef, buildPos);
			}
		}

		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		if ((buildPos != -RgtVector) && terrainManager->CanBeBuiltAt(bestDef, buildPos) &&
			((unit == nullptr) || terrainManager->CanBuildAt(unit, buildPos)))
		{
			IBuilderTask::Priority priority = isEnergyStalling ? IBuilderTask::Priority::HIGH : IBuilderTask::Priority::NORMAL;
			return builderManager->EnqueueTask(priority, bestDef, buildPos, IBuilderTask::BuildType::ENERGY, cost, SQUARE_SIZE * 16.0f);
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
	float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome()) * ecoFactor;
	CCircuitDef* assistDef = factoryManager->GetAssistDef();
	float factoryFactor = (metalIncome - assistDef->GetBuildSpeed()) * 1.2f;
	const int nanoSize = builderManager->GetTasks(IBuilderTask::BuildType::NANO).size();
	float factoryPower = factoryManager->GetFactoryPower() + nanoSize * assistDef->GetBuildSpeed();
	if ((factoryPower >= factoryFactor) || !builderManager->GetTasks(IBuilderTask::BuildType::FACTORY).empty()) {
		return nullptr;
	}

	// check nanos
	CCircuitUnit* factory = factoryManager->NeedUpgrade();
	if ((factory != nullptr) && assistDef->IsAvailable()) {
		AIFloat3 buildPos = factory->GetPos(circuit->GetLastFrame());
		switch (factory->GetUnit()->GetBuildingFacing()) {
			default:
			case UNIT_FACING_SOUTH:
				buildPos.z -= 128.0f;  // def->GetZSize() * SQUARE_SIZE * 2;
				break;
			case UNIT_FACING_EAST:
				buildPos.x -= 128.0f;  // def->GetXSize() * SQUARE_SIZE * 2;
				break;
			case UNIT_FACING_NORTH:
				buildPos.z += 128.0f;  // def->GetZSize() * SQUARE_SIZE * 2;
				break;
			case UNIT_FACING_WEST:
				buildPos.x += 128.0f;  // def->GetXSize() * SQUARE_SIZE * 2;
				break;
		}

		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		CCircuitDef* bdef = (unit == nullptr) ? factory->GetCircuitDef() : unit->GetCircuitDef();
		buildPos = terrainManager->GetBuildPosition(bdef, buildPos);

		if (terrainManager->CanBeBuiltAt(assistDef, buildPos) &&
			((unit == nullptr) || terrainManager->CanBuildAt(unit, buildPos)))
		{
			return builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, assistDef, buildPos,
											   IBuilderTask::BuildType::NANO, SQUARE_SIZE * 8, true);
		}
	}

	// check factories
	CCircuitDef* facDef = factoryManager->GetFactoryToBuild(circuit);
	if (facDef != nullptr) {
		CMetalData::MetalPredicate predicate = [this](const CMetalData::MetalNode& v) {
			return clusterInfos[v.second].factory == nullptr;
		};
		CMetalManager* metalManager = circuit->GetMetalManager();
		int index = metalManager->FindNearestCluster(position, predicate);
		if (index >= 0) {
			const CMetalData::Clusters& clusters = metalManager->GetClusters();
			AIFloat3 buildPos = clusters[index].geoCentr;

			CTerrainManager* terrainManager = circuit->GetTerrainManager();
			AIFloat3 center = AIFloat3(terrainManager->GetTerrainWidth() / 2, 0, terrainManager->GetTerrainHeight() / 2);
			float size = (center.SqDistance2D(circuit->GetSetupManager()->GetStartPos()) > center.SqDistance2D(buildPos)) ? -200.0f : 200.0f;  // std::max(facUDef->GetXSize(), facUDef->GetZSize()) * SQUARE_SIZE;
			buildPos.x += (buildPos.x > center.x) ? -size : size;
			buildPos.z += (buildPos.z > center.z) ? -size : size;

			// identify area to build by factory representatives
			CCircuitDef* bdef;
			CCircuitDef* landDef = factoryManager->GetLandDef(facDef);
			if (landDef != nullptr) {
				if (landDef->GetMobileId() < 0) {
					bdef = landDef;
				} else {
					STerrainMapArea* area = terrainManager->GetMobileTypeById(landDef->GetMobileId())->areaLargest;
					bdef = ((area == nullptr) || (area->percentOfMap < 40.0)) ? factoryManager->GetWaterDef(facDef) : landDef;
				}
			} else {
				bdef = factoryManager->GetWaterDef(facDef);
			}
			if (bdef == nullptr) {
				return nullptr;
			}

			buildPos = terrainManager->GetBuildPosition(bdef, buildPos);

			if (terrainManager->CanBeBuiltAt(facDef, buildPos) &&
				((unit == nullptr) || terrainManager->CanBuildAt(unit, buildPos)))
			{
				return builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, facDef, buildPos,
												   IBuilderTask::BuildType::FACTORY);
			}
		}
	}

	return nullptr;
}

IBuilderTask* CEconomyManager::UpdateFactoryTasks()
{
	return UpdateFactoryTasks(circuit->GetSetupManager()->GetBasePos());
}

IBuilderTask* CEconomyManager::UpdateStorageTasks()
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask()) {
		return nullptr;
	}

	CCircuitDef* storeDef = circuit->GetCircuitDef("armmstor");

	float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome());
	if (!builderManager->GetTasks(IBuilderTask::BuildType::STORE).empty() ||
		(economy->GetStorage(metalRes) > 10 * metalIncome) ||
		(storeDef->GetCount() >= 5) ||
		!storeDef->IsAvailable())
	{
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
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		int terWidth = terrainManager->GetTerrainWidth();
		int terHeight = terrainManager->GetTerrainHeight();
		float x = terWidth / 4 + rand() % (int)(terWidth / 2 + 1);
		float z = terHeight / 4 + rand() % (int)(terHeight / 2 + 1);
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

	float cost = pylonDef->GetCost();
	unsigned count = builderManager->GetBuilderPower() / cost * 8 + 1;
	if (builderManager->GetTasks(IBuilderTask::BuildType::PYLON).size() < count) {
		energyGrid->Update();

		CCircuitDef* buildDef;
		AIFloat3 buildPos;
		CEnergyLink* link = energyGrid->GetLinkToBuild(buildDef, buildPos);
		if (link != nullptr) {
			if ((buildPos != -RgtVector) && builderManager->IsBuilderInArea(buildDef, buildPos)) {
				return builderManager->EnqueuePylon(IBuilderTask::Priority::HIGH, buildDef, buildPos, link, cost);
			} else {
				link->SetValid(false);  // FIXME: Reset valid on timer? Or when air con appears
				energyGrid->SetForceRebuild(true);
			}
		}
	}

	return nullptr;
}

void CEconomyManager::AddMorphee(CCircuitUnit* unit)
{
	morphees.insert(unit);
	if (morph == nullptr) {
		morph = std::make_shared<CGameTask>(&CEconomyManager::UpdateMorph, this);
		circuit->GetScheduler()->RunTaskEvery(morph, FRAMES_PER_SEC * 10);
	}
}

void CEconomyManager::UpdateMorph()
{
	if (morphees.empty()) {
		circuit->GetScheduler()->RemoveTask(morph);
		morph = nullptr;
		return;
	}

	float energyIncome = GetAvgEnergyIncome();
	float metalIncome = std::min(GetAvgMetalIncome(), energyIncome);
	if ((metalIncome < 10) || !IsMetalFull() || (GetMetalPull() * 0.8f > metalIncome)) {
		return;
	}

	auto it = morphees.begin();
	while (it != morphees.end()) {
		CCircuitUnit* unit = *it;
		if (unit->GetUnit()->GetHealth() < unit->GetUnit()->GetMaxHealth() * 0.8f) {
			++it;
		} else {
			unit->Upgrade();  // Morph();
			it = morphees.erase(it);
			break;  // one unit at a time
		}
	}
}

void CEconomyManager::ReadConfig()
{
	// Using cafus, armfus, armsolar as control points
	// FIXME: Дабы ветка параболы заработала надо использовать [x <= 0; y < min limit) для точки перегиба
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const Json::Value& energy = root["energy"];

	incomeFactor = energy.get("income_factor", 1.0f).asFloat();

	std::array<std::pair<std::string, int>, 3> landEngies;
	const Json::Value& land = energy["land"];
	unsigned li = 0;
	for (const std::string& engy : land.getMemberNames()) {
		const int min = land[engy][0].asInt();
		const int max = land[engy].get(1, min).asInt();
		const int limit = min + rand() % (max - min + 1);
		landEngies[li++] = std::make_pair(engy, limit);
		if (li >= 3) {
			break;
		}
	}

	std::array<std::pair<std::string, int>, 3> waterEngies;
	const Json::Value& water = energy["water"];
	unsigned wi = 0;
	for (const std::string& engy : water.getMemberNames()) {
		const int min = water[engy][0].asInt();
		const int max = water[engy].get(1, min).asInt();
		const int limit = min + rand() % (max - min + 1);
		waterEngies[wi++] = std::make_pair(engy, limit);
		if (wi >= 3) {
			break;
		}
	}

	bool isWaterMap = circuit->GetTerrainManager()->GetPercentLand() < 40.0;
	std::array<std::pair<std::string, int>, 3>& engies = isWaterMap ? waterEngies : landEngies;
	CLagrangeInterPol::Vector x(engies.size()), y(engies.size());
	for (unsigned i = 0; i < engies.size(); ++i) {
		CCircuitDef* cdef = circuit->GetCircuitDef(engies[i].first.c_str());
		float make = utils::string_to_float(cdef->GetUnitDef()->GetCustomParams().find("income_energy")->second);
		x[i] = cdef->GetCost() / make;
		y[i] = engies[i].second + 0.5;  // +0.5 to be sure precision errors will not decrease integer part
	}
	engyPol = new CLagrangeInterPol(x, y);  // Alternatively use CGaussSolver to compute polynomial - faster on reuse

	// NOTE: Alternative to CLagrangeInterPol
//	CGaussSolver solve;
//	CGaussSolver::Matrix a = {
//		{1, x[0], x[0]*x[0]},
//		{1, x[1], x[1]*x[1]},
//		{1, x[2], x[2]*x[2]}
//	};
//	CGaussSolver::Vector r = solve.Solve(a, y);
//	circuit->LOG("y = %f*x^0 + %f*x^1 + %f*x^2", r[0], r[1], r[2]);
}

void CEconomyManager::Init()
{
	const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
	clusterInfos.resize(clusters.size(), {nullptr});

	auto subinit = [this]() {
		CScheduler* scheduler = circuit->GetScheduler().get();
		CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
		if (commander != nullptr) {
			AIFloat3 buildPos = -RgtVector;
			const AIFloat3& pos = commander->GetPos(circuit->GetLastFrame());
			UnitRulesParam* param = commander->GetUnit()->GetUnitRulesParamByName("facplop");
			if ((param != nullptr) && (param->GetValueFloat() == 1)) {
				CFactoryManager* factoryManager = circuit->GetFactoryManager();
				CCircuitDef* facDef = factoryManager->GetFactoryToBuild(circuit, true);
				if (facDef != nullptr) {
					// Enqueue factory
					CTerrainManager* terrainManager = circuit->GetTerrainManager();
					buildPos = terrainManager->GetBuildPosition(facDef, pos);
					CBuilderManager* builderManager = circuit->GetBuilderManager();
					IBuilderTask* task = builderManager->EnqueueTask(IBuilderTask::Priority::NOW, facDef, buildPos,
													   IBuilderTask::BuildType::FACTORY);
					static_cast<ITaskManager*>(builderManager)->AssignTask(commander, task);

					// Enqueue first builder
					float radius = std::max(terrainManager->GetTerrainWidth(), terrainManager->GetTerrainHeight()) / 4;
					CCircuitDef* buildDef = factoryManager->GetRoleDef(facDef, CCircuitDef::RoleType::BUILDER);
					factoryManager->EnqueueTask(CRecruitTask::Priority::NORMAL, buildDef, buildPos, CRecruitTask::RecruitType::BUILDPOWER, radius);
				}
			}
			delete param;

			scheduler->RunTaskAt(std::make_shared<CGameTask>([this, buildPos]() {
				// Force commander level 0 to morph
				CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
				if (commander != nullptr) {
					const std::map<std::string, std::string>& customParams = commander->GetCircuitDef()->GetUnitDef()->GetCustomParams();
					auto it = customParams.find("level");
					if ((it != customParams.end()) && (utils::string_to_int(it->second) <= 1)) {
						commander->Upgrade();  // Morph();
					}
				}
				// Build factory defence
				if (buildPos != -RgtVector) {
					circuit->GetBuilderManager()->EnqueueTask(IBuilderTask::Priority::NORMAL, circuit->GetCircuitDef("corllt"), buildPos,
															  IBuilderTask::BuildType::DEFENCE, true, true, 0);
				}
			}), FRAMES_PER_SEC * 120);
		}

		SkirmishAIs* ais = circuit->GetCallback()->GetSkirmishAIs();
		const int interval = ais->GetSize() * FRAMES_PER_SEC;
		delete ais;
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(static_cast<IBuilderTask* (CEconomyManager::*)(void)>(&CEconomyManager::UpdateFactoryTasks), this),
								interval, circuit->GetSkirmishAIId() + 0 + 10 * interval);
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateStorageTasks, this), interval, circuit->GetSkirmishAIId() + 1);
	};
	// Try to avoid blocked factories on start
	circuit->GetScheduler()->RunTaskAfter(std::make_shared<CGameTask>(subinit), circuit->GetSkirmishAIId() * 2);
}

void CEconomyManager::UpdateEconomy()
{
	if (ecoFrame + TEAM_SLOWUPDATE_RATE >= circuit->GetLastFrame()) {
		return;
	}
	ecoFrame = circuit->GetLastFrame();

	float curMetal = economy->GetCurrent(metalRes);
	float storMetal = economy->GetStorage(metalRes);
	isMetalEmpty = curMetal < storMetal * 0.2f;
	isMetalFull = curMetal > storMetal * 0.8f;
	isEnergyStalling = std::min(GetAvgMetalIncome() - GetMetalPull(), .0f) * 0.98f > std::min(GetAvgEnergyIncome() - GetEnergyPull(), .0f);
	float curEnergy = economy->GetCurrent(energyRes);
	float storEnergy = economy->GetStorage(energyRes) - HIDDEN_ENERGY;
	isEnergyEmpty = curEnergy < storEnergy * 0.1f;
}

} // namespace circuit
