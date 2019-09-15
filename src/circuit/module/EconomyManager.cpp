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
#include "Resource.h"
#include "Economy.h"
#include "Feature.h"
#include "FeatureDef.h"
#include "Team.h"
#include "Log.h"

namespace circuit {

using namespace springai;

#define PYLON_RANGE		500.0f

CEconomyManager::CEconomyManager(CCircuitAI* circuit)
		: IModule(circuit)
		, energyGrid(nullptr)
		, pylonDef(nullptr)
		, mexDef(nullptr)
		, storeDef(nullptr)
		, mexCount(0)
		, lastFacFrame(-1)
		, indexRes(0)
		, metalIncome(.0f)
		, energyIncome(.0f)
		, metalProduced(.0f)
		, metalUsed(.0f)
		, ecoFrame(-1)
		, isMetalEmpty(false)
		, isMetalFull(false)
		, isEnergyStalling(false)
		, isEnergyEmpty(false)
		, metalPullFrame(-1)
		, energyPullFrame(-1)
		, energyUseFrame(-1)
		, metalPull(.0f)
		, energyPull(.0f)
		, energyUse(.0f)
{
	metalRes = circuit->GetCallback()->GetResourceByName("Metal");
	energyRes = circuit->GetCallback()->GetResourceByName("Energy");
	economy = circuit->GetCallback()->GetEconomy();

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
		lastFacFrame = this->circuit->GetLastFrame();
		int index = this->circuit->GetMetalManager()->FindNearestCluster(unit->GetPos(lastFacFrame));
		if (index >= 0) {
			clusterInfos[index].factory = unit;
		}
	};
	auto factoryDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		if (unit->GetTask()->GetType() == IUnitTask::Type::NIL) {
			return;
		}
		for (auto& info : clusterInfos) {
			if (info.factory == unit) {
				info.factory = nullptr;
			}
		}
	};

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
		float income = unit->GetUnit()->GetRulesParamFloat("mexIncome", 0.f);
		for (int i = 0; i < INCOME_SAMPLES; ++i) {
			metalIncomes[i] += income;
		}
		metalIncome += income;
	};

	/*
	 * morph & plop
	 */
	auto comFinishedHandler = [this](CCircuitUnit* unit) {
		AddMorphee(unit);
		this->circuit->GetSetupManager()->SetCommander(unit);

		ICoreUnit::Id unitId = unit->GetId();
		this->circuit->GetScheduler()->RunTaskAfter(std::make_shared<CGameTask>([this, unitId]() {
			CCircuitUnit* unit = this->circuit->GetTeamUnit(unitId);
			if (unit == nullptr) {
				return;
			}
			int frame = this->circuit->GetLastFrame();
			bool isStart = (frame < FRAMES_PER_SEC * 10);
			AIFloat3 buildPos = -RgtVector;
			if (unit->GetUnit()->GetRulesParamFloat("facplop", 0) == 1) {
				const AIFloat3& pos = unit->GetPos(frame);
				CCircuitDef* facDef = this->circuit->GetFactoryManager()->GetFactoryToBuild(pos, isStart);
				if (facDef != nullptr) {
					// Enqueue factory
					CTerrainManager* terrainManager = this->circuit->GetTerrainManager();
					buildPos = terrainManager->GetBuildPosition(facDef, pos);
					CBuilderManager* builderManager = this->circuit->GetBuilderManager();
					IBuilderTask* task = builderManager->EnqueueFactory(IBuilderTask::Priority::NOW, facDef, buildPos,
																		SQUARE_SIZE * 32, true, true, 0);
					static_cast<ITaskManager*>(builderManager)->AssignTask(unit, task);

					if (builderManager->GetWorkerCount() <= 2) {
						OpenStrategy(facDef, buildPos);
					}
				}
			}

			if (!isStart) {
				return;
			}
			int morphFrame = this->circuit->GetSetupManager()->GetMorphFrame(unit->GetCircuitDef());
			if (morphFrame >= 0) {
				this->circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>([this, unitId]() {
					// Force commander level 0 to morph
					CCircuitUnit* unit = this->circuit->GetTeamUnit(unitId);
					if ((unit != nullptr) && (unit->GetTask() != nullptr) &&
						(unit->GetTask()->GetType() != IUnitTask::Type::PLAYER))
					{
						const std::map<std::string, std::string>& customParams = unit->GetCircuitDef()->GetUnitDef()->GetCustomParams();
						auto it = customParams.find("level");
						if ((it != customParams.end()) && (utils::string_to_int(it->second) <= 1)) {
							unit->Upgrade();  // Morph();
						}
					}
				}), morphFrame);
			}
		}), FRAMES_PER_SEC);
	};
	auto comDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		RemoveMorphee(unit);

		CSetupManager* setupManager = this->circuit->GetSetupManager();
		CCircuitUnit* commander = setupManager->GetCommander();
		if (commander == unit) {
			setupManager->SetCommander(nullptr);
		}
	};

	float maxAreaDivCost = .0f;
	float maxStoreDivCost = .0f;
	CCircuitDef* commDef = circuit->GetSetupManager()->GetCommChoice();

	const CCircuitAI::CircuitDefs& allDefs = circuit->GetCircuitDefs();
	for (auto& kv : allDefs) {
		CCircuitDef* cdef = kv.second;
		const std::map<std::string, std::string>& customParams = cdef->GetUnitDef()->GetCustomParams();

		if (!cdef->IsMobile()) {
			if (commDef->CanBuild(cdef)) {
				// pylon
				auto it = customParams.find("pylonrange");
				if (it != customParams.end()) {
					const float range = utils::string_to_float(it->second);
					float areaDivCost = M_PI * SQUARE(range) / cdef->GetCost();
					if (maxAreaDivCost < areaDivCost) {
						maxAreaDivCost = areaDivCost;
						pylonDef = cdef;  // armestor
						pylonRange = range;
					}
				}

				// storage
				float storeDivCost = cdef->GetUnitDef()->GetStorage(metalRes) / cdef->GetCost();
				if (maxStoreDivCost < storeDivCost) {
					maxStoreDivCost = storeDivCost;
					storeDef = cdef;  // armmstor
				}

				// mex
				// BA: float metalConverts = unitDef->GetMakesResource(metalRes);
				//     float metalExtracts = unitDef->GetExtractsResource(metalRes);
				//     float netMetal = unitDef->GetResourceMake(metalRes) - unitDef->GetUpkeep(metalRes);
				if (((it = customParams.find("ismex")) != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
					finishedHandler[kv.first] = mexFinishedHandler;
					mexDef = cdef;  // cormex
				}
			}

			// factory
			if (cdef->GetUnitDef()->IsBuilder() && !cdef->GetBuildOptions().empty()) {
				finishedHandler[cdef->GetId()] = factoryFinishedHandler;
				destroyedHandler[cdef->GetId()] = factoryDestroyedHandler;
			}

			// energy
			// BA: float netEnergy = unitDef->GetResourceMake(energyRes) - unitDef->GetUpkeep(energyRes);
			auto it = customParams.find("income_energy");
			if ((it != customParams.end()) && (utils::string_to_float(it->second) > 1)) {
				finishedHandler[kv.first] = energyFinishedHandler;
				allEnergyDefs.insert(cdef);
			}

		} else {

			// commander
			if (cdef->IsRoleComm()) {
				finishedHandler[cdef->GetId()] = comFinishedHandler;
				destroyedHandler[cdef->GetId()] = comDestroyedHandler;
			}
		}
	}

	ReadConfig();

	if (mexDef == nullptr) {
		mexDef = defaultDef;
	}
	if (pylonDef == nullptr) {
		pylonDef = defaultDef;
		pylonRange = PYLON_RANGE;
	}
}

CEconomyManager::~CEconomyManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete metalRes;
	delete energyRes;
	delete economy;
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

CCircuitDef* CEconomyManager::GetLowEnergy(const AIFloat3& pos, float& outMake) const
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CCircuitDef* result = nullptr;
	const int frame = circuit->GetLastFrame();
	auto it = energyInfos.rbegin();
	while (it != energyInfos.rend()) {
		CCircuitDef* candy = it->cdef;
		if (candy->IsAvailable(frame) && terrainManager->CanBeBuiltAtSafe(candy, pos)) {
			result = candy;
			outMake = it->make;
			break;
		}
		++it;
	}
	return result;
}

void CEconomyManager::AddEnergyDefs(const std::set<CCircuitDef*>& buildDefs)
{
	// TODO: Cache engyDefs in CCircuitDef?
	std::set<CCircuitDef*> engyDefs;
	std::set_intersection(allEnergyDefs.begin(), allEnergyDefs.end(),
						  buildDefs.begin(), buildDefs.end(),
						  std::inserter(engyDefs, engyDefs.begin()));
	if (engyDefs.empty()) {
		return;
	}
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
		engy.make = utils::string_to_float(cdef->GetUnitDef()->GetCustomParams().find("income_energy")->second);
		engy.costDivMake = engy.cost / engy.make;
		engy.limit = engyPol->GetValueAt(engy.costDivMake);
		energyInfos.push_back(engy);
	}

	// High-tech energy first
	auto compare = [](const SEnergyInfo& e1, const SEnergyInfo& e2) {
		return e1.costDivMake < e2.costDivMake;
	};
	std::sort(energyInfos.begin(), energyInfos.end(), compare);
}

void CEconomyManager::RemoveEnergyDefs(const std::set<CCircuitDef*>& buildDefs)
{
	// TODO: Cache engyDefs in CCircuitDef?
	std::set<CCircuitDef*> engyDefs;
	std::set_intersection(allEnergyDefs.begin(), allEnergyDefs.end(),
						  buildDefs.begin(), buildDefs.end(),
						  std::inserter(engyDefs, engyDefs.begin()));
	if (engyDefs.empty()) {
		return;
	}
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
		} else {
			++it;
		}
	}
}

void CEconomyManager::UpdateResourceIncome()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	float oddEnergyIncome = circuit->GetTeam()->GetRulesParamFloat("OD_energyIncome", 0.f);
	float oddEnergyChange = circuit->GetTeam()->GetRulesParamFloat("OD_energyChange", 0.f);

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

	metalProduced += metalIncome * metalMod;
	metalUsed += economy->GetUsage(metalRes);
}

float CEconomyManager::GetMetalPull()
{
	if (metalPullFrame/* + TEAM_SLOWUPDATE_RATE*/ < circuit->GetLastFrame()) {
		metalPullFrame = circuit->GetLastFrame();
		metalPull = economy->GetPull(metalRes) + circuit->GetTeam()->GetRulesParamFloat("extraMetalPull", 0.f);
	}
	return metalPull;
}

float CEconomyManager::GetEnergyPull()
{
	if (energyPullFrame/* + TEAM_SLOWUPDATE_RATE*/ < circuit->GetLastFrame()) {
		energyPullFrame = circuit->GetLastFrame();
		float extraEnergyPull = circuit->GetTeam()->GetRulesParamFloat("extraEnergyPull", 0.f);
//		float oddEnergyOverdrive = circuit->GetTeam()->GetRulesParamFloat("OD_energyOverdrive", 0.f);
//		float oddEnergyChange = circuit->GetTeam()->GetRulesParamFloat("OD_energyChange", 0.f);
//		float extraChange = std::min(.0f, oddEnergyChange) - std::min(.0f, oddEnergyOverdrive);
//		float teamEnergyWaste = circuit->GetTeam()->GetRulesParamFloat("OD_team_energyWaste", 0.f);
//		float numAllies = circuit->GetTeam()->GetRulesParamFloat("OD_allies", 1.f);
//		if (numAllies < 1.f) {
//			numAllies = 1.f;
//		}
		energyPull = economy->GetPull(energyRes) + extraEnergyPull/* + extraChange - teamEnergyWaste / numAllies*/;
	}
	return energyPull;
}

float CEconomyManager::GetEnergyUse()
{
	if (energyUseFrame/* + TEAM_SLOWUPDATE_RATE*/ < circuit->GetLastFrame()) {
		energyUseFrame = circuit->GetLastFrame();
		energyUse = economy->GetUsage(energyRes);
	}
	return energyUse;
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

bool CEconomyManager::IsAllyOpenSpot(int spotId) const
{
	return IsOpenSpot(spotId) && circuit->GetMetalManager()->IsOpenSpot(spotId);
}

void CEconomyManager::SetOpenSpot(int spotId, bool value)
{
	if (openSpots[spotId] == value) {
		return;
	}
	openSpots[spotId] = value;
	value ? --mexCount : ++mexCount;
}

bool CEconomyManager::IsIgnorePull(const IBuilderTask* task) const
{
	if (mexMax != std::numeric_limits<decltype(mexMax)>::max()) {
		return false;
	}
	return ((task->GetBuildType() == IBuilderTask::BuildType::MEX) ||
			(task->GetBuildType() == IBuilderTask::BuildType::PYLON));
}

bool CEconomyManager::IsIgnoreStallingPull(const IBuilderTask* task) const
{
	if (mexMax != std::numeric_limits<decltype(mexMax)>::max()) {
		return false;
	}
	if ((task->GetBuildType() == IBuilderTask::BuildType::MEX) ||
		(task->GetBuildType() == IBuilderTask::BuildType::PYLON))
	{
		return true;
	}
	return ((task->GetBuildType() == IBuilderTask::BuildType::ENERGY) &&
			circuit->GetEconomyManager()->IsEnergyStalling());
}

IBuilderTask* CEconomyManager::MakeEconomyTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask()) {
		return nullptr;
	}
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(position);
	if ((index < 0) || (clusterInfos[index].metalFrame + FRAMES_PER_SEC >= circuit->GetLastFrame())) {
		return nullptr;
	}
	clusterInfos[index].metalFrame = circuit->GetLastFrame();

	IBuilderTask* task = UpdateMetalTasks(position, unit);
	if (task != nullptr) {
		return task;
	}

	return task;
}

IBuilderTask* CEconomyManager::UpdateMetalTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask(16)) {
		return nullptr;
	}

	IBuilderTask* task = nullptr;

	// check uncolonized mexes
	bool isEnergyStalling = IsEnergyStalling();
	if (!isEnergyStalling && mexDef->IsAvailable(circuit->GetLastFrame())) {
		float cost = mexDef->GetCost();
		unsigned maxCount = builderManager->GetBuildPower() / cost * 8 + 2;
		if (builderManager->GetTasks(IBuilderTask::BuildType::MEX).size() < maxCount) {
			CMetalManager* metalManager = circuit->GetMetalManager();
			CTerrainManager* terrainManager = circuit->GetTerrainManager();
			const CMetalData::Metals& spots = metalManager->GetSpots();
			Map* map = circuit->GetMap();
			CMetalManager::MexPredicate predicate;
			if (unit != nullptr) {
				CCircuitDef* mexDef = this->mexDef;
				predicate = [this, &spots, map, mexDef, terrainManager, unit](int index) {
					return (IsAllyOpenSpot(index) &&
							terrainManager->CanBeBuiltAtSafe(mexDef, spots[index].position) &&  // hostile environment
							terrainManager->CanBuildAtSafe(unit, spots[index].position) &&
							map->IsPossibleToBuildAt(mexDef->GetUnitDef(), spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
				};
			} else {
				CCircuitDef* mexDef = this->mexDef;
				predicate = [this, &spots, map, mexDef, terrainManager, builderManager](int index) {
					return (IsAllyOpenSpot(index) &&
							terrainManager->CanBeBuiltAtSafe(mexDef, spots[index].position) &&  // hostile environment
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
				SetOpenSpot(index, false);
				return task;
			}
		}
	}

	task = isEnergyStalling ? UpdateEnergyTasks(position, unit) : UpdateReclaimTasks(position, unit);

	return task;
}

IBuilderTask* CEconomyManager::UpdateReclaimTasks(const AIFloat3& position, CCircuitUnit* unit, bool isNear)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (/*!builderManager->CanEnqueueTask() || */(unit == nullptr)) {
		return nullptr;
	}

	if (IsMetalFull() || (builderManager->GetTasks(IBuilderTask::BuildType::RECLAIM).size() >= builderManager->GetWorkerCount() / 2)) {
		return nullptr;
	}

	std::vector<Feature*> features;
	if (isNear) {
		const float distance = unit->GetCircuitDef()->GetSpeed() * ((GetMetalPull() * 0.8f > GetAvgMetalIncome()) ? 300 : 30);
		features = std::move(circuit->GetCallback()->GetFeaturesIn(position, distance));
	} else {
		features = std::move(circuit->GetCallback()->GetFeatures());
	}
	if (features.empty()) {
		return nullptr;
	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	AIFloat3 pos;
	float cost = .0f;
	float minSqDist = std::numeric_limits<float>::max();
	for (Feature* feature : features) {
		AIFloat3 featPos = feature->GetPosition();
		CTerrainManager::CorrectPosition(featPos);  // Impulsed flying feature
		if (!terrainManager->CanBuildAtSafe(unit, featPos)) {
			continue;
		}
		FeatureDef* featDef = feature->GetDef();
		if (!featDef->IsReclaimable()) {
			delete featDef;
			continue;
		}
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
	IBuilderTask* task = nullptr;
	if (minSqDist < std::numeric_limits<float>::max()) {
		for (IBuilderTask* t : builderManager->GetTasks(IBuilderTask::BuildType::RECLAIM)) {
			if (utils::is_equal_pos(pos, t->GetTaskPos())) {
				task = t;
				break;
			}
		}
		if (task == nullptr) {
			task = builderManager->EnqueueReclaim(IBuilderTask::Priority::HIGH, pos, cost, FRAMES_PER_SEC * 300,
												  8.0f/*unit->GetCircuitDef()->GetBuildDistance()*/);
		}
	}
	utils::free_clear(features);

	return task;
}

IBuilderTask* CEconomyManager::UpdateEnergyTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask(32)) {
		return nullptr;
	}

	// check energy / metal ratio
	float metalIncome = GetAvgMetalIncome();
	const float energyIncome = GetAvgEnergyIncome();
	const bool isEnergyStalling = IsEnergyStalling();

	// Select proper energy UnitDef to build
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CCircuitDef* bestDef = nullptr;
	CCircuitDef* hopeDef = nullptr;
	bool isLastHope = isEnergyStalling;
	metalIncome = std::min(metalIncome, energyIncome) * energyFactor;
	const float buildPower = std::min(builderManager->GetBuildPower(), metalIncome);
	const int taskSize = builderManager->GetTasks(IBuilderTask::BuildType::ENERGY).size();
	const float maxBuildTime = MAX_BUILD_SEC * (isEnergyStalling ? 0.25f : ecoFactor);

	const int frame = circuit->GetLastFrame();
	for (const SEnergyInfo& engy : energyInfos) {  // sorted by high-tech first
		// TODO: Add geothermal powerplant support
		if (!engy.cdef->IsAvailable(frame) ||
			!terrainManager->CanBeBuiltAtSafe(engy.cdef, position) ||
			engy.cdef->GetUnitDef()->IsNeedGeo())
		{
			continue;
		}

		if (engy.cdef->GetCount() < engy.limit) {
			isLastHope = false;
			if (taskSize < (int)(buildPower / engy.cost * 8 + 1)) {
				bestDef = engy.cdef;
				// TODO: Select proper scale/quadratic function (x*x) and smoothing coefficient (8).
				//       МЕТОД НАИМЕНЬШИХ КВАДРАТОВ ! (income|buildPower, make/cost) - points
				//       solar       geothermal    fusion         singu           ...
				//       (10, 2/70), (15, 25/500), (20, 35/1000), (30, 225/4000), ...
				if (engy.cost * 16.0f < maxBuildTime * SQUARE(buildPower)) {
					break;
				}
			} else if (engy.cost * 16.0f < maxBuildTime * SQUARE(buildPower)) {
				bestDef = nullptr;
				break;
			}
		} else if (!isEnergyStalling) {
			bestDef = nullptr;
			break;
		} else if (hopeDef == nullptr) {
			hopeDef = engy.cdef;
			isLastHope = isLastHope && (taskSize < (int)(buildPower / hopeDef->GetCost() * 8 + 1));
		}
	}
	if (isLastHope) {
		bestDef = hopeDef;
	}

	if (bestDef == nullptr) {
		return nullptr;
	}

	// Find place to build
	AIFloat3 buildPos = -RgtVector;
	CMetalManager* metalManager = circuit->GetMetalManager();
	if (bestDef->GetCost() < 1000.0f) {
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
			buildPos = clusters[index].position;

			// TODO: Calc enemy vector and move position into opposite direction
			AIFloat3 mapCenter(circuit->GetTerrainManager()->GetTerrainWidth() / 2, 0, circuit->GetTerrainManager()->GetTerrainHeight() / 2);
			buildPos += (buildPos - mapCenter).Normalize2D() * 300.0f * (bestDef->GetCost() / energyInfos.front().cost);
			CCircuitDef* bdef = (unit == nullptr) ? bestDef : unit->GetCircuitDef();
			CTerrainManager::CorrectPosition(buildPos);
			buildPos = circuit->GetTerrainManager()->GetBuildPosition(bdef, buildPos);
		}
	}

	if (utils::is_valid(buildPos) && terrainManager->CanBeBuiltAtSafe(bestDef, buildPos) &&
		((unit == nullptr) || terrainManager->CanBuildAtSafe(unit, buildPos)))
	{
		IBuilderTask::Priority priority = isEnergyStalling ? IBuilderTask::Priority::HIGH : IBuilderTask::Priority::NORMAL;
		return builderManager->EnqueueTask(priority, bestDef, buildPos, IBuilderTask::BuildType::ENERGY, SQUARE_SIZE * 16.0f, true);
	}

	return nullptr;
}

IBuilderTask* CEconomyManager::UpdateFactoryTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask(64)) {
		return nullptr;
	}

	/*
	 * check air pads
	 */
	CFactoryManager* factoryManager = circuit->GetFactoryManager();
	CMilitaryManager* militaryManager = circuit->GetMilitaryManager();
	CCircuitDef* airpadDef = factoryManager->GetAirpadDef();
	const std::set<IBuilderTask*> &factoryTasks = builderManager->GetTasks(IBuilderTask::BuildType::FACTORY);
	const unsigned airpadFactor = SQUARE((airpadDef->GetCount() + factoryTasks.size()) * 4);
	const int frame = circuit->GetLastFrame();
	if (airpadDef->IsAvailable(frame) &&
		(militaryManager->GetRoleUnits(CCircuitDef::RoleType::BOMBER).size() > airpadFactor))
	{
		CCircuitDef* bdef;
		AIFloat3 buildPos;
		if (unit == nullptr) {
			bdef = airpadDef;
			buildPos = factoryManager->GetClosestHaven(circuit->GetSetupManager()->GetBasePos());
		} else {
			bdef = unit->GetCircuitDef();
			buildPos = factoryManager->GetClosestHaven(unit);
		}
		if (!utils::is_valid(buildPos)) {
			buildPos = circuit->GetSetupManager()->GetBasePos();
		}
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		buildPos = terrainManager->GetBuildPosition(bdef, buildPos);

		if (terrainManager->CanBeBuiltAtSafe(airpadDef, buildPos) &&
			((unit == nullptr) || terrainManager->CanBuildAtSafe(unit, buildPos)))
		{
			return builderManager->EnqueueFactory(IBuilderTask::Priority::NORMAL, airpadDef, buildPos);
		}
	}

	/*
	 * check buildpower
	 */
	const float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome()) * ecoFactor;
	CCircuitDef* assistDef = factoryManager->GetAssistDef();
	const float factoryFactor = (metalIncome - assistDef->GetBuildSpeed()) * 1.2f;
	const int nanoSize = builderManager->GetTasks(IBuilderTask::BuildType::NANO).size();
	const float factoryPower = factoryManager->GetFactoryPower() + nanoSize * assistDef->GetBuildSpeed();
	const bool isSwitchTime = (lastFacFrame + switchTime <= frame);
	if ((factoryPower >= factoryFactor) && !isSwitchTime) {
		return nullptr;
	}

	/*
	 * check nanos
	 */
	if (!isSwitchTime) {
		CCircuitUnit* factory = factoryManager->NeedUpgrade();
		if ((factory != nullptr) && assistDef->IsAvailable(frame)) {
			AIFloat3 buildPos = factory->GetPos(frame);
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
			CTerrainManager::CorrectPosition(buildPos);
			buildPos = terrainManager->GetBuildPosition(bdef, buildPos);

			if (terrainManager->CanBeBuiltAtSafe(assistDef, buildPos) &&
				((unit == nullptr) || terrainManager->CanBuildAtSafe(unit, buildPos)))
			{
				return builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, assistDef, buildPos,
												   IBuilderTask::BuildType::NANO, SQUARE_SIZE * 8, true);
			}
		}
	}

	/*
	 * check factories
	 */
	if (!factoryTasks.empty()) {
		return nullptr;
	}

	const AIFloat3& enemyPos = militaryManager->GetEnemyPos();
	AIFloat3 pos(circuit->GetSetupManager()->GetBasePos());
	CMetalManager* metalManager = circuit->GetMetalManager();
	AIFloat3 center = (pos + enemyPos) * 0.5f;
	float minSqDist = std::numeric_limits<float>::max();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	for (unsigned i = 0; i < clusters.size(); ++i) {
		if (!metalManager->IsClusterFinished(i)) {
			continue;
		}
		const float sqDist = center.SqDistance2D(clusters[i].position);
		if (minSqDist > sqDist) {
			minSqDist = sqDist;
			pos = clusters[i].position;
		}
	}

	CMetalData::PointPredicate predicate = [this](const int index) {
		return clusterInfos[index].factory == nullptr;
	};
	int index = metalManager->FindNearestCluster(pos, predicate);
	if (index < 0) {
		return nullptr;
	}
	AIFloat3 buildPos = clusters[index].position;

	const bool isStart = (factoryManager->GetFactoryCount() == 0);
	CCircuitDef* facDef = factoryManager->GetFactoryToBuild(buildPos, isStart);
	if (facDef == nullptr) {
		return nullptr;
	}
	if (facDef->IsRoleSupport()) {
		buildPos = circuit->GetSetupManager()->GetBasePos();
	}

	const float sqStartDist = enemyPos.SqDistance2D(circuit->GetSetupManager()->GetStartPos());
	const float sqBuildDist = enemyPos.SqDistance2D(buildPos);
	float size = (sqStartDist > sqBuildDist) ? -200.0f : 200.0f;  // std::max(facUDef->GetXSize(), facUDef->GetZSize()) * SQUARE_SIZE;
	buildPos.x += (buildPos.x > enemyPos.x) ? -size : size;
	buildPos.z += (buildPos.z > enemyPos.z) ? -size : size;

	// identify area to build by factory representatives
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CCircuitDef* bdef;
	CCircuitDef* landDef = factoryManager->GetLandDef(facDef);
	if (landDef != nullptr) {
		if (landDef->GetMobileId() < 0) {
			bdef = landDef;
		} else {
			STerrainMapArea* area = terrainManager->GetMobileTypeById(landDef->GetMobileId())->areaLargest;
			// FIXME: area->percentOfMap < 40.0 doesn't seem right as water identifier
			bdef = ((area == nullptr) || (area->percentOfMap < 40.0)) ? factoryManager->GetWaterDef(facDef) : landDef;
		}
	} else {
		bdef = factoryManager->GetWaterDef(facDef);
	}
	if (bdef == nullptr) {
		return nullptr;
	}

	CTerrainManager::CorrectPosition(buildPos);
	buildPos = terrainManager->GetBuildPosition(bdef, buildPos);

	if (terrainManager->CanBeBuiltAtSafe(facDef, buildPos) &&
		((unit == nullptr) || terrainManager->CanBuildAtSafe(unit, buildPos)))
	{
		lastFacFrame = frame;
		IBuilderTask::Priority priority = (builderManager->GetWorkerCount() <= 2) ?
										  IBuilderTask::Priority::NOW :
										  IBuilderTask::Priority::HIGH;
		return builderManager->EnqueueFactory(priority, facDef, buildPos);
	}

	return nullptr;
}

IBuilderTask* CEconomyManager::UpdateFactoryTasks()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	return UpdateFactoryTasks(circuit->GetSetupManager()->GetBasePos());
}

IBuilderTask* CEconomyManager::UpdateStorageTasks()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (!builderManager->CanEnqueueTask()) {
		return nullptr;
	}

	const float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome());
	if ((storeDef == nullptr) ||
		!builderManager->GetTasks(IBuilderTask::BuildType::STORE).empty() ||
		(GetStorage(metalRes) > 10 * metalIncome) ||
		!storeDef->IsAvailable(circuit->GetLastFrame()))
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
		float x = terWidth / 4 + rand() % (int)(terWidth / 2);
		float z = terHeight / 4 + rand() % (int)(terHeight / 2);
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

	const float energyIncome = GetAvgEnergyIncome();
	const float metalIncome = std::min(GetAvgMetalIncome(), energyIncome);
	if ((metalIncome < 10) || (energyIncome < 80) || !pylonDef->IsAvailable(circuit->GetLastFrame())) {
		return nullptr;
	}

	const float cost = pylonDef->GetCost();
	unsigned count = builderManager->GetBuildPower() / cost * 8 + 1;
	if (builderManager->GetTasks(IBuilderTask::BuildType::PYLON).size() >= count) {
		return nullptr;
	}

	energyGrid->Update();

	CCircuitDef* buildDef;
	AIFloat3 buildPos;
	CEnergyLink* link = energyGrid->GetLinkToBuild(buildDef, buildPos);
	if ((link == nullptr) || (buildDef == nullptr)) {
		return nullptr;
	}

	if (utils::is_valid(buildPos) && builderManager->IsBuilderInArea(buildDef, buildPos)) {
		return builderManager->EnqueuePylon(IBuilderTask::Priority::HIGH, buildDef, buildPos, link, cost);
	} else {
		link->SetValid(false);
		energyGrid->SetForceRebuild(true);
		// TODO: Optimize: when invalid link appears start watchdog gametask
		//       that will traverse invalidLinks vector and enable link on timeout.
		//       When invalidLinks is empty remove watchdog gametask.
		circuit->GetScheduler()->RunTaskAfter(std::make_shared<CGameTask>([link](CEnergyGrid* energyGrid) {
			link->SetValid(true);
			energyGrid->SetForceRebuild(true);
		}, energyGrid), FRAMES_PER_SEC * 120);
	}

	return nullptr;
}

void CEconomyManager::AddMorphee(CCircuitUnit* unit)
{
	if (!unit->IsUpgradable() || (unit->GetTask() == nullptr)) {
		return;
	}
	morphees.insert(unit);
	if (morph == nullptr) {
		morph = std::make_shared<CGameTask>(&CEconomyManager::UpdateMorph, this);
		circuit->GetScheduler()->RunTaskEvery(morph, FRAMES_PER_SEC * 10);
	}
}

void CEconomyManager::UpdateMorph()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	if (morphees.empty()) {
		circuit->GetScheduler()->RemoveTask(morph);
		morph = nullptr;
		return;
	}

	const float energyIncome = GetAvgEnergyIncome();
	const float metalIncome = std::min(GetAvgMetalIncome(), energyIncome);
	if ((metalIncome < 10) || !IsExcessed() || !IsMetalFull() || (GetMetalPull() * 0.8f > metalIncome)) {
		return;
	}

	auto it = morphees.begin();
	while (it != morphees.end()) {
		CCircuitUnit* unit = *it;
		if ((unit->GetTask()->GetType() == IUnitTask::Type::PLAYER) ||
			(unit->GetUnit()->GetHealth() < unit->GetUnit()->GetMaxHealth() * 0.8f))
		{
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
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();
	const Json::Value& econ = root["economy"];
	ecoStep = econ.get("eps_step", 0.25f).asFloat();
	ecoFactor = (circuit->GetAllyTeam()->GetSize() - 1.0f) * ecoStep + 1.0f;
	metalMod = (1.f - econ.get("excess", -1.f).asFloat());
	switchTime = econ.get("switch", 900).asInt() * FRAMES_PER_SEC;
	const float bd = econ.get("build_delay", -1.f).asFloat();
	buildDelay = (bd > 0.f) ? (bd * FRAMES_PER_SEC) : 0;

	const Json::Value& energy = econ["energy"];
	const Json::Value& factor = energy["factor"];
	efInfo.startFactor = factor[0].get((unsigned)0, 0.5f).asFloat();
	efInfo.startFrame = factor[0].get((unsigned)1, 300 ).asInt() * FRAMES_PER_SEC;
	efInfo.endFactor = factor[1].get((unsigned)0, 2.0f).asFloat();
	efInfo.endFrame = factor[1].get((unsigned)1, 3600).asInt() * FRAMES_PER_SEC;
	efInfo.fraction = (efInfo.endFactor - efInfo.startFactor) / (efInfo.endFrame - efInfo.startFrame);
	energyFactor = efInfo.startFactor;

	// Using cafus, armfus, armsolar as control points
	// FIXME: Дабы ветка параболы заработала надо использовать [x <= 0; y < min limit) для точки перегиба
	constexpr unsigned MAX_CTRL_POINTS = 3;
	std::vector<std::pair<std::string, int>> engies;
	std::string type = circuit->GetTerrainManager()->IsWaterMap() ? "water" : "land";
	const Json::Value& surf = energy[type];
	unsigned si = 0;
	for (const std::string& engy : surf.getMemberNames()) {
		const int min = surf[engy][0].asInt();
		const int max = surf[engy].get(1, min).asInt();
		const int limit = min + rand() % (max - min + 1);
		engies.push_back(std::make_pair(engy, limit));
		if (++si >= MAX_CTRL_POINTS) {
			break;
		}
	}

	CLagrangeInterPol::Vector x(engies.size()), y(engies.size());
	for (unsigned i = 0; i < engies.size(); ++i) {
		CCircuitDef* cdef = circuit->GetCircuitDef(engies[i].first.c_str());
		if (cdef == nullptr) {
			circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), engies[i].first.c_str());
			continue;
		}
		const std::map<std::string, std::string>& customParams = cdef->GetUnitDef()->GetCustomParams();
		auto it = customParams.find("income_energy");
		float make = (it != customParams.end()) ? utils::string_to_float(it->second) : 1.f;
		x[i] = cdef->GetCost() / make;
		y[i] = engies[i].second + 0.5;  // +0.5 to be sure precision errors will not decrease integer part
	}
	engyPol = new CLagrangeInterPol(x, y);  // Alternatively use CGaussSolver to compute polynomial - faster on reuse

	// NOTE: Alternative to CLagrangeInterPol
//	CGaussSolver solver;
//	CGaussSolver::Matrix a = {
//		{1, x[0], x[0]*x[0]},
//		{1, x[1], x[1]*x[1]},
//		{1, x[2], x[2]*x[2]}
//	};
//	CGaussSolver::Vector r = solver.Solve(a, y);
//	circuit->LOG("y = %f*x^0 + %f*x^1 + %f*x^2", r[0], r[1], r[2]);

	// NOTE: Must have
	defaultDef = circuit->GetCircuitDef(econ["default"].asCString());
	if (defaultDef == nullptr) {
		throw CException("economy.default");
	}
}

void CEconomyManager::Init()
{
	energyGrid = circuit->GetAllyTeam()->GetEnergyGrid().get();

	const size_t clSize = circuit->GetMetalManager()->GetClusters().size();
	clusterInfos.resize(clSize, {nullptr, -FRAMES_PER_SEC});
	const size_t spSize = circuit->GetMetalManager()->GetSpots().size();
	openSpots.resize(spSize, true);

	const Json::Value& econ = circuit->GetSetupManager()->GetConfig()["economy"];
	const float mm = econ.get("mex_max", 2.f).asFloat();
	mexMax = (mm < 1.f) ? (mm * spSize) : std::numeric_limits<decltype(mexMax)>::max();

	const Json::Value& pull = econ["ms_pull"];
	mspInfos.resize(pull.size());
	mspInfos.push_back(SPullMtoS {
		.pull = pull[0].get((unsigned)0, 1.0f).asFloat(),
		.mex = (int)(pull[0].get((unsigned)1, 0.0f).asFloat() * spSize),
		.fraction = 0.f
	});
	for (unsigned i = 1; i < pull.size(); ++i) {
		SPullMtoS mspInfoEnd;
		mspInfoEnd.pull = pull[i].get((unsigned)0, 0.25f).asFloat();
		mspInfoEnd.mex = pull[i].get((unsigned)1, 0.75f).asFloat() * spSize;
		mspInfoEnd.fraction = 0.f;
		mspInfos.push_back(mspInfoEnd);
		SPullMtoS& mspInfoBegin = mspInfos[i - 1];
		mspInfoBegin.fraction = (mspInfoEnd.pull - mspInfoBegin.pull) / (mspInfoEnd.mex - mspInfoBegin.mex);
	}
	std::sort(mspInfos.begin(), mspInfos.end());
	pullMtoS = mspInfos.front().pull;

	CSetupManager::StartFunc subinit = [this](const AIFloat3& pos) {
		metalProduced = economy->GetCurrent(metalRes) * metalMod;

		CScheduler* scheduler = circuit->GetScheduler().get();
		CAllyTeam* allyTeam = circuit->GetAllyTeam();
		if (circuit->IsCommMerge()) {
			const int spotId = circuit->GetMetalManager()->FindNearestSpot(pos);
			const int clusterId = (spotId < 0) ? -1 : circuit->GetMetalManager()->GetCluster(spotId);
			int ownerId = allyTeam->GetClusterTeam(clusterId).teamId;
			if (ownerId < 0) {
				ownerId = circuit->GetTeamId();
				allyTeam->OccupyCluster(clusterId, ownerId);
			} else if (ownerId != circuit->GetTeamId()) {
				// Resign
				std::vector<Unit*> migrants;
				for (auto& kv : circuit->GetTeamUnits()) {
					migrants.push_back(kv.second->GetUnit());
				}
				economy->SendUnits(migrants, ownerId);
				circuit->Resign(ownerId);
				return;
			}
		}

		scheduler->RunTaskAfter(std::make_shared<CGameTask>([this]() {
			ecoFactor = (circuit->GetAllyTeam()->GetAliveSize() - 1.0f) * ecoStep + 1.0f;
		}), FRAMES_PER_SEC * 10);

		const int interval = allyTeam->GetSize() * FRAMES_PER_SEC;
		auto update = static_cast<IBuilderTask* (CEconomyManager::*)(void)>(&CEconomyManager::UpdateFactoryTasks);
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(update, this),
								interval, circuit->GetSkirmishAIId() + 0 + 10 * interval);
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateStorageTasks, this),
								interval, circuit->GetSkirmishAIId() + 1 + interval / 2);
	};

	circuit->GetSetupManager()->ExecOnFindStart(subinit);
}

void CEconomyManager::OpenStrategy(CCircuitDef* facDef, const AIFloat3& pos)
{
	CFactoryManager* factoryManager = circuit->GetFactoryManager();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	float radius = std::max(terrainManager->GetTerrainWidth(), terrainManager->GetTerrainHeight()) / 4;
	const std::vector<CCircuitDef::RoleType>* opener = circuit->GetSetupManager()->GetOpener(facDef);
	if (opener == nullptr) {
		return;
	}
	for (CCircuitDef::RoleType type : *opener) {
		CCircuitDef* buildDef = factoryManager->GetRoleDef(facDef, type);
		if ((buildDef == nullptr) || !buildDef->IsAvailable(circuit->GetLastFrame())) {
			continue;
		}
		CRecruitTask::Priority priotiry;
		CRecruitTask::RecruitType recruit;
		if (type == CCircuitDef::RoleType::BUILDER) {
			priotiry = CRecruitTask::Priority::NORMAL;
			recruit  = CRecruitTask::RecruitType::BUILDPOWER;
		} else {
			priotiry = CRecruitTask::Priority::HIGH;
			recruit  = CRecruitTask::RecruitType::FIREPOWER;
		}
		factoryManager->EnqueueTask(priotiry, buildDef, pos, recruit, radius);
	}
}

float CEconomyManager::GetStorage(Resource* res)
{
	return economy->GetStorage(res) - HIDDEN_STORAGE;
}

void CEconomyManager::UpdateEconomy()
{
	if (ecoFrame + TEAM_SLOWUPDATE_RATE >= circuit->GetLastFrame()) {
		return;
	}
	ecoFrame = circuit->GetLastFrame();

	const float curMetal = economy->GetCurrent(metalRes);
	const float storMetal = GetStorage(metalRes);
	isMetalEmpty = curMetal < storMetal * 0.2f;
	isMetalFull = curMetal > storMetal * 0.8f;
	isEnergyStalling = std::min(GetAvgMetalIncome() - GetMetalPull(), .0f)/* * 0.98f*/ > std::min(GetAvgEnergyIncome() - GetEnergyPull(), .0f);
	const float curEnergy = economy->GetCurrent(energyRes);
	const float storEnergy = GetStorage(energyRes);
	isEnergyEmpty = curEnergy < storEnergy * 0.1f;

	if (ecoFrame <= efInfo.startFrame) {
		energyFactor = efInfo.startFactor;
	} else if (ecoFrame >= efInfo.endFrame) {
		energyFactor = efInfo.endFactor;
	} else {
		energyFactor = efInfo.fraction * (ecoFrame - efInfo.startFrame) + efInfo.startFactor;
	}

	if (mexCount <= mspInfos.front().mex) {
		pullMtoS = mspInfos.front().pull;
	} else if (mexCount >= mspInfos.back().mex) {
		pullMtoS = mspInfos.back().pull;
	} else {
		auto it = std::lower_bound(mspInfos.cbegin(), mspInfos.cend(), mexCount, SPullMtoS());
		SPullMtoS& mspInfo = mspInfos[std::distance(mspInfos.cbegin(), it)];
		pullMtoS = mspInfo.fraction * (mexCount - mspInfo.mex) + mspInfo.pull;
	}
	pullMtoS *= circuit->GetMilitaryManager()->ClampMobileCostRatio();
}

} // namespace circuit
