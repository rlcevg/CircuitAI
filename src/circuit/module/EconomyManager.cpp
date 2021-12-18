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
#include "script/EconomyScript.h"
#include "setup/SetupManager.h"
#include "resource/MetalManager.h"
#include "resource/EnergyGrid.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/Utils.h"
#include "json/json.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "AISCommands.h"
#include "Resource.h"
#include "Economy.h"
#include "Feature.h"
#include "FeatureDef.h"
#include "Team.h"
#include "Log.h"

namespace circuit {

using namespace springai;

#define PYLON_RANGE		500.0f

const char* RES_NAME_METAL = "Metal";
const char* RES_NAME_ENERGY = "Energy";

CEconomyManager::CEconomyManager(CCircuitAI* circuit)
		: IModule(circuit, new CEconomyScript(circuit->GetScriptManager(), this))
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
		, metalCurFrame(-1)
		, metalPullFrame(-1)
		, energyCurFrame(-1)
		, energyPullFrame(-1)
		, energyUseFrame(-1)
		, metalCur(.0f)
		, metalPull(.0f)
		, energyCur(.0f)
		, energyPull(.0f)
		, energyUse(.0f)
{
	metalRes = circuit->GetCallback()->GetResourceByName(RES_NAME_METAL);
	energyRes = circuit->GetCallback()->GetResourceByName(RES_NAME_ENERGY);
	economy = circuit->GetCallback()->GetEconomy();

	metalIncomes.resize(INCOME_SAMPLES, 4.0f);  // Init metal income
	energyIncomes.resize(INCOME_SAMPLES, 6.0f);  // Init energy income

	// TODO: Use A* ai planning... or sth... STRIPS https://ru.wikipedia.org/wiki/STRIPS
	//       https://ru.wikipedia.org/wiki/Марковский_процесс_принятия_решений

	circuit->GetScheduler()->RunOnInit(std::make_shared<CGameTask>(&CEconomyManager::Init, this));

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
	auto factoryDestroyedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
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
		const SEnergyExt* energyExt = energyDefs.GetAvailInfo(unit->GetCircuitDef());
		if (energyExt != nullptr) {
			const float income = energyExt->make;
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
				// TODO: check factory's customparam ploppable=1
				const AIFloat3& pos = unit->GetPos(frame);
				CCircuitDef* facDef = this->circuit->GetFactoryManager()->GetFactoryToBuild(pos, isStart);
				if (facDef != nullptr) {
					// Enqueue factory
					CTerrainManager* terrainMgr = this->circuit->GetTerrainManager();
					buildPos = terrainMgr->GetBuildPosition(facDef, pos);
					CBuilderManager* builderMgr = this->circuit->GetBuilderManager();
					IBuilderTask* task = builderMgr->EnqueueFactory(IBuilderTask::Priority::NOW, facDef, buildPos,
																	SQUARE_SIZE, true, true, 0);
					static_cast<ITaskManager*>(builderMgr)->AssignTask(unit, task);
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
						const std::map<std::string, std::string>& customParams = unit->GetCircuitDef()->GetDef()->GetCustomParams();
						auto it = customParams.find("level");
						if ((it != customParams.end()) && (utils::string_to_int(it->second) <= 1)) {
							unit->Upgrade();  // Morph();
						}
					}
				}), morphFrame);
			}
		}), FRAMES_PER_SEC);
	};
	auto comDestroyedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
		RemoveMorphee(unit);

		CSetupManager* setupMgr = this->circuit->GetSetupManager();
		CCircuitUnit* commander = setupMgr->GetCommander();
		if (commander == unit) {
			setupMgr->SetCommander(nullptr);
		}
	};

	float maxAreaDivCost = .0f;
	float maxStoreDivCost = .0f;
	CCircuitDef* commDef = circuit->GetSetupManager()->GetCommChoice();

	for (CCircuitDef& cdef : circuit->GetCircuitDefs()) {
		const std::map<std::string, std::string>& customParams = cdef.GetDef()->GetCustomParams();

		if (!cdef.IsMobile()) {
			if (commDef->CanBuild(&cdef)) {
				// pylon
				auto it = customParams.find("pylonrange");
				if (it != customParams.end()) {
					const float range = utils::string_to_float(it->second);
					float areaDivCost = M_PI * SQUARE(range) / cdef.GetCostM();
					if (maxAreaDivCost < areaDivCost) {
						maxAreaDivCost = areaDivCost;
						pylonDef = &cdef;  // armestor
						pylonRange = range;
					}
				}

				// storage
				float storeDivCost = cdef.GetDef()->GetStorage(metalRes) / cdef.GetCostM();
				if (maxStoreDivCost < storeDivCost) {
					maxStoreDivCost = storeDivCost;
					storeDef = &cdef;  // armmstor
				}

				// mex
				// BA: float metalConverts = unitDef->GetMakesResource(metalRes);
				//     float metalExtracts = unitDef->GetExtractsResource(metalRes);
				//     float netMetal = unitDef->GetResourceMake(metalRes) - unitDef->GetUpkeep(metalRes);
				if (((it = customParams.find("ismex")) != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
					finishedHandler[cdef.GetId()] = mexFinishedHandler;
					mexDef = &cdef;  // cormex
					cdef.SetIsMex(true);
				}
			}

			// factory
			if (cdef.GetDef()->IsBuilder() && !cdef.GetBuildOptions().empty()) {
				finishedHandler[cdef.GetId()] = factoryFinishedHandler;
				destroyedHandler[cdef.GetId()] = factoryDestroyedHandler;
			}

			// energy
			// BA: float netEnergy = unitDef->GetResourceMake(energyRes) - unitDef->GetUpkeep(energyRes);
			auto it = customParams.find("income_energy");
			if ((it != customParams.end()) && (utils::string_to_float(it->second) > 1)) {
				finishedHandler[cdef.GetId()] = energyFinishedHandler;
				energyDefs.AddDef(&cdef);
			}

		} else {

			// commander
			if (cdef.IsRoleComm()) {
				finishedHandler[cdef.GetId()] = comFinishedHandler;
				destroyedHandler[cdef.GetId()] = comDestroyedHandler;
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
	delete metalRes;
	delete energyRes;
	delete economy;
}

void CEconomyManager::ReadConfig()
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();
	const Json::Value& econ = root["economy"];
	ecoStep = econ.get("eps_step", 0.25f).asFloat();
	ecoFactor = (circuit->GetAllyTeam()->GetSize() - 1.0f) * ecoStep + 1.0f;
	metalMod = (1.f - econ.get("excess", -1.f).asFloat());
	const Json::Value& swch = econ["switch"];
	const int minSwitch = swch.get((unsigned)0, 800).asInt();
	const int maxSwitch = swch.get((unsigned)1, 900).asInt();
	switchTime = (minSwitch + rand() % (maxSwitch - minSwitch + 1)) * FRAMES_PER_SEC;

	{
		const Json::Value& bd = econ["build_delay"];
		float value = bd[0].get((unsigned)0, -1.f).asFloat();
		bdInfo.startDelay = (value > 0.f) ? (value * FRAMES_PER_SEC) : 0;
		bdInfo.startFrame = bd[0].get((unsigned)1, 0).asInt() * FRAMES_PER_SEC;
		value = bd[1].get((unsigned)0, -1.f).asFloat();
		bdInfo.endDelay = (value > 0.f) ? (value * FRAMES_PER_SEC) : 0;
		bdInfo.endFrame = bd[1].get((unsigned)1, 0).asInt() * FRAMES_PER_SEC;
		bdInfo.fraction = (bdInfo.endFrame != bdInfo.startFrame)
				? float(bdInfo.endDelay - bdInfo.startDelay) / (bdInfo.endFrame - bdInfo.startFrame)
				: 0.f;
		buildDelay = bdInfo.startDelay;
	}

	const Json::Value& energy = econ["energy"];
	{
		const Json::Value& factor = energy["factor"];
		efInfo.startFactor = factor[0].get((unsigned)0, 0.5f).asFloat();
		efInfo.startFrame = factor[0].get((unsigned)1, 300 ).asInt() * FRAMES_PER_SEC;
		efInfo.endFactor = factor[1].get((unsigned)0, 2.0f).asFloat();
		efInfo.endFrame = factor[1].get((unsigned)1, 3600).asInt() * FRAMES_PER_SEC;
		efInfo.fraction = (efInfo.endFrame != efInfo.startFrame)
				? (efInfo.endFactor - efInfo.startFactor) / (efInfo.endFrame - efInfo.startFrame)
				: 0.f;
		energyFactor = efInfo.startFactor;
	}

	std::vector<std::pair<std::string, int>> engies;
	std::string type = circuit->GetTerrainManager()->IsWaterMap() ? "water" : "land";
	const Json::Value& surf = energy[type];
	for (const std::string& engy : surf.getMemberNames()) {
		const int min = surf[engy][0].asInt();
		const int max = surf[engy].get(1, min).asInt();
		const int limit = min + rand() % (max - min + 1);
		engies.push_back(std::make_pair(engy, limit));
	}

	for (unsigned i = 0; i < engies.size(); ++i) {
		const char* name = engies[i].first.c_str();
		CCircuitDef* cdef = circuit->GetCircuitDef(name);
		if (cdef == nullptr) {
			circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), name);
			continue;
		}
		engyLimits[cdef] = engies[i].second;
	}

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
		if (circuit->IsCommMerge() && !circuit->IsLoadSave()) {
			const int spotId = circuit->GetMetalManager()->FindNearestSpot(pos);
			const int clusterId = (spotId < 0) ? -1 : circuit->GetMetalManager()->GetCluster(spotId);
			int ownerId = allyTeam->GetClusterTeam(clusterId).teamId;
			if (ownerId < 0) {
				ownerId = circuit->GetTeamId();
				allyTeam->OccupyCluster(clusterId, ownerId);
			} else if (ownerId != circuit->GetTeamId()) {
				// Resign
				circuit->Resign(ownerId, economy);
				return;
			}
			// FIXME: DEBUG
//			CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
//			if (commander == nullptr) {
//				commander = circuit->GetTeamUnits().begin()->second;
//			}
//			int ownerId = allyTeam->GetAreaTeam(commander->GetArea()).teamId;
//			if (ownerId < 0) {
//				ownerId = circuit->GetTeamId();
//				allyTeam->OccupyArea(commander->GetArea(), ownerId);
//			} else if (ownerId != circuit->GetTeamId()) {
//				// Resign
//				circuit->Resign(ownerId, economy);
//				return;
//			}
			// FIXME: DEBUG
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

		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CEconomyManager::UpdateResourceIncome, this), TEAM_SLOWUPDATE_RATE);
	};

	circuit->GetSetupManager()->ExecOnFindStart(subinit);
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

int CEconomyManager::UnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	// NOTE: If more actions should be done then consider moving into damagedHandler
	if (unit->IsMorphing() && (unit->GetUnit()->GetHealth() < unit->GetUnit()->GetMaxHealth() * 0.5f)) {
		unit->StopUpgrade();  // StopMorph();
		AddMorphee(unit);
	}

	return 0; //signaling: OK
}

int CEconomyManager::UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

CCircuitDef* CEconomyManager::GetLowEnergy(const AIFloat3& pos, float& outMake) const
{
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	const int frame = circuit->GetLastFrame();
	return energyDefs.GetWorstDef([frame, terrainMgr, &pos, &outMake](CCircuitDef* cdef, const SEnergyExt& data) {
		if (cdef->IsAvailable(frame) && terrainMgr->CanBeBuiltAtSafe(cdef, pos)) {
			outMake = data.make;
			return true;
		}
		return false;
	});
}

void CEconomyManager::AddEnergyDefs(const std::set<CCircuitDef*>& buildDefs)
{
	energyDefs.AddDefs(buildDefs, [this](CCircuitDef* cdef, SEnergyExt& data) -> float {
		data.make = utils::string_to_float(cdef->GetDef()->GetCustomParams().find("income_energy")->second);
		auto lit = engyLimits.find(cdef);
		data.limit = (lit != engyLimits.end()) ? lit->second : 0;
		// TODO: Instead of plain sizeX, sizeZ use AI's yardmap size
		return SQUARE(data.make) / ((cdef->GetCostM()/* + cdef->GetCostE() * 0.05f*/) * cdef->GetDef()->GetXSize() * cdef->GetDef()->GetZSize());
	});

	// DEBUG
//	for (const auto& ei : energyDefs.GetInfos()) {
//		circuit->LOG("%s | costM=%f | costE=%f | make=%f | efficiency=%f | limit=%i", ei.cdef->GetDef()->GetName(),
//				ei.cdef->GetCostM(), ei.cdef->GetCostE(), ei.data.make, ei.score, ei.data.limit);
//	}
}

void CEconomyManager::RemoveEnergyDefs(const std::set<CCircuitDef*>& buildDefs)
{
	energyDefs.RemoveDefs(buildDefs);

	// DEBUG
//	for (const auto& ei : energyDefs.GetInfos()) {
//		circuit->LOG("%s | costM=%f | costE=%f | make=%f | efficiency=%f | limit=%i", ei.cdef->GetDef()->GetName(),
//				ei.cdef->GetCostM(), ei.cdef->GetCostE(), ei.data.make, ei.score, ei.data.limit);
//	}
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

float CEconomyManager::GetMetalCur()
{
	if (metalCurFrame/* + TEAM_SLOWUPDATE_RATE*/ < circuit->GetLastFrame()) {
		metalCurFrame = circuit->GetLastFrame();
		metalCur = economy->GetCurrent(metalRes);
	}
	return metalCur;
}

float CEconomyManager::GetMetalPull()
{
	if (metalPullFrame/* + TEAM_SLOWUPDATE_RATE*/ < circuit->GetLastFrame()) {
		metalPullFrame = circuit->GetLastFrame();
		metalPull = economy->GetPull(metalRes) + circuit->GetTeam()->GetRulesParamFloat("extraMetalPull", 0.f);
	}
	return metalPull;
}

float CEconomyManager::GetEnergyCur()
{
	if (energyCurFrame/* + TEAM_SLOWUPDATE_RATE*/ < circuit->GetLastFrame()) {
		energyCurFrame = circuit->GetLastFrame();
		energyCur = economy->GetCurrent(energyRes);
	}
	return energyCur;
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
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	if (!builderMgr->CanEnqueueTask()) {
		return nullptr;
	}
	CMetalManager* metalMgr = circuit->GetMetalManager();
	int index = metalMgr->FindNearestCluster(position);
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
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	if (!builderMgr->CanEnqueueTask(16)) {
		return nullptr;
	}

	IBuilderTask* task = nullptr;

	// check uncolonized mexes
	bool isEnergyStalling = IsEnergyStalling();
	if (!isEnergyStalling && mexDef->IsAvailable(circuit->GetLastFrame())) {
		float cost = mexDef->GetCostM();
		unsigned maxCount = builderMgr->GetBuildPower() / cost * 8 + 2;
		if (builderMgr->GetTasks(IBuilderTask::BuildType::MEX).size() < maxCount) {
			CMetalManager* metalMgr = circuit->GetMetalManager();
			CTerrainManager* terrainMgr = circuit->GetTerrainManager();
			const CMetalData::Metals& spots = metalMgr->GetSpots();
			CMap* map = circuit->GetMap();
			CMetalData::PointPredicate predicate;
			if (unit != nullptr) {
				CCircuitDef* mexDef = this->mexDef;
				predicate = [this, &spots, map, mexDef, terrainMgr, unit](int index) {
					return (IsAllyOpenSpot(index)
							&& terrainMgr->CanBeBuiltAtSafe(mexDef, spots[index].position)  // hostile environment
							&& terrainMgr->CanReachAtSafe(unit, spots[index].position, unit->GetCircuitDef()->GetBuildDistance())
							&& map->IsPossibleToBuildAt(mexDef->GetDef(), spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
				};
			} else {
				CCircuitDef* mexDef = this->mexDef;
				predicate = [this, &spots, map, mexDef, terrainMgr, builderMgr](int index) {
					return (IsAllyOpenSpot(index)
							&& terrainMgr->CanBeBuiltAtSafe(mexDef, spots[index].position)  // hostile environment
							&& builderMgr->IsBuilderInArea(mexDef, spots[index].position)
							&& map->IsPossibleToBuildAt(mexDef->GetDef(), spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
				};
			}
			int index = metalMgr->GetMexToBuild(position, predicate);
			if (index != -1) {
				const AIFloat3& pos = spots[index].position;
				task = builderMgr->EnqueueTask(IBuilderTask::Priority::HIGH, mexDef, pos, IBuilderTask::BuildType::MEX, cost, .0f);
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
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	if (/*!builderManager->CanEnqueueTask() || */(unit == nullptr)) {
		return nullptr;
	}

	if (IsMetalFull() || (builderMgr->GetTasks(IBuilderTask::BuildType::RECLAIM).size() >= builderMgr->GetWorkerCount() / 2)) {
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

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	AIFloat3 pos;
	float cost = .0f;
	float minSqDist = std::numeric_limits<float>::max();
	for (Feature* feature : features) {
		AIFloat3 featPos = feature->GetPosition();
		CTerrainManager::CorrectPosition(featPos);  // Impulsed flying feature
		if (!terrainMgr->CanReachAtSafe(unit, featPos, unit->GetCircuitDef()->GetBuildDistance())) {
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
		for (IBuilderTask* t : builderMgr->GetTasks(IBuilderTask::BuildType::RECLAIM)) {
			if (utils::is_equal_pos(pos, t->GetTaskPos())) {
				task = t;
				break;
			}
		}
		if (task == nullptr) {
			task = builderMgr->EnqueueReclaim(IBuilderTask::Priority::HIGH, pos, cost, FRAMES_PER_SEC * 300,
											  8.0f/*unit->GetCircuitDef()->GetBuildDistance()*/);
		}
	}
	utils::free_clear(features);

	return task;
}

IBuilderTask* CEconomyManager::UpdateEnergyTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	if (!builderMgr->CanEnqueueTask(32)) {
		return nullptr;
	}

	// check energy / metal ratio
	float metalIncome = GetAvgMetalIncome();
	const float energyIncome = GetAvgEnergyIncome();
	const bool isEnergyStalling = IsEnergyStalling();
	// TODO: e-stalling needs separate array of energy-defs sorted by cost

	// Select proper energy UnitDef to build
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CCircuitDef* bestDef = nullptr;
	CCircuitDef* hopeDef = nullptr;
	bool isLastHope = isEnergyStalling;
	metalIncome = std::min(metalIncome, energyIncome) * energyFactor;
	const float buildPower = std::min(builderMgr->GetBuildPower(), metalIncome);
	const int taskSize = builderMgr->GetTasks(IBuilderTask::BuildType::ENERGY).size();
	const float maxBuildTime = MAX_BUILD_SEC * (isEnergyStalling ? 0.25f : 1.f);

	const int frame = circuit->GetLastFrame();
	for (const auto& engy : energyDefs.GetInfos()) {  // sorted by high-tech first
		// TODO: Add geothermal powerplant support
		if (!engy.cdef->IsAvailable(frame) ||
			!terrainMgr->CanBeBuiltAtSafe(engy.cdef, position) ||
			engy.cdef->GetDef()->IsNeedGeo())
		{
			continue;
		}

		if (engy.cdef->GetCount() < engy.data.limit) {
			isLastHope = false;
			if (taskSize < (int)(buildPower / engy.cdef->GetCostM() * 8 + 1)) {
				bestDef = engy.cdef;
				// TODO: Select proper scale/quadratic function (x*x) and smoothing coefficient (8).
				//       МЕТОД НАИМЕНЬШИХ КВАДРАТОВ ! (income|buildPower, make/cost) - points
				//       solar       geothermal    fusion         singu           ...
				//       (10, 2/70), (15, 25/500), (20, 35/1000), (30, 225/4000), ...
				if (engy.cdef->GetCostM() * 16.0f < maxBuildTime * SQUARE(metalIncome)) {
					break;
				}
			} else if (engy.cdef->GetCostM() * 16.0f < maxBuildTime * SQUARE(metalIncome)) {
				bestDef = nullptr;
				break;
			}
		} else if (!isEnergyStalling) {
			bestDef = nullptr;
			break;
		} else if (hopeDef == nullptr) {
			hopeDef = engy.cdef;
			isLastHope = isLastHope && (taskSize < (int)(buildPower / hopeDef->GetCostM() * 8 + 1));
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
	CMetalManager* metalMgr = circuit->GetMetalManager();
	if (bestDef->GetCostM() < 1000.0f) {
		int index = metalMgr->FindNearestSpot(position);
		if (index != -1) {
			const CMetalData::Metals& spots = metalMgr->GetSpots();
			buildPos = spots[index].position;
		}
	} else {
		const AIFloat3& startPos = circuit->GetSetupManager()->GetBasePos();
		int index = metalMgr->FindNearestCluster(startPos);
		if (index >= 0) {
			const CMetalData::Clusters& clusters = metalMgr->GetClusters();
			buildPos = clusters[index].position;

			// TODO: Calc enemy vector and move position into opposite direction
			AIFloat3 mapCenter(circuit->GetTerrainManager()->GetTerrainWidth() / 2, 0, circuit->GetTerrainManager()->GetTerrainHeight() / 2);
			buildPos += (buildPos - mapCenter).Normalize2D() * 300.0f * (bestDef->GetCostM() / energyDefs.GetFirstDef()->GetCostM());
			CCircuitDef* bdef = (unit == nullptr) ? bestDef : unit->GetCircuitDef();
			CTerrainManager::CorrectPosition(buildPos);
			buildPos = circuit->GetTerrainManager()->GetBuildPosition(bdef, buildPos);
		}
	}

	if (utils::is_valid(buildPos) && terrainMgr->CanBeBuiltAtSafe(bestDef, buildPos) &&
		((unit == nullptr) || terrainMgr->CanReachAtSafe(unit, buildPos, unit->GetCircuitDef()->GetBuildDistance())))
	{
		IBuilderTask::Priority priority = isEnergyStalling ? IBuilderTask::Priority::HIGH : IBuilderTask::Priority::NORMAL;
		return builderMgr->EnqueueTask(priority, bestDef, buildPos, IBuilderTask::BuildType::ENERGY, SQUARE_SIZE * 16.0f, true);
	}

	return nullptr;
}

IBuilderTask* CEconomyManager::UpdateFactoryTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	if (!builderMgr->CanEnqueueTask(64)) {
		return nullptr;
	}

	/*
	 * check air pads
	 */
	CFactoryManager* factoryMgr = circuit->GetFactoryManager();
	CMilitaryManager* militaryMgr = circuit->GetMilitaryManager();
	CCircuitDef* airpadDef = factoryMgr->GetAirpadDef();
	const std::set<IBuilderTask*> &factoryTasks = builderMgr->GetTasks(IBuilderTask::BuildType::FACTORY);
	const unsigned airpadFactor = SQUARE((airpadDef->GetCount() + factoryTasks.size()) * 4);
	const int frame = circuit->GetLastFrame();
	if (airpadDef->IsAvailable(frame) &&
		(militaryMgr->GetRoleUnits(ROLE_TYPE(BOMBER)).size() > airpadFactor))
	{
		CCircuitDef* bdef;
		AIFloat3 buildPos;
		if (unit == nullptr) {
			bdef = airpadDef;
			buildPos = factoryMgr->GetClosestHaven(circuit->GetSetupManager()->GetBasePos());
		} else {
			bdef = unit->GetCircuitDef();
			buildPos = factoryMgr->GetClosestHaven(unit);
		}
		if (!utils::is_valid(buildPos)) {
			buildPos = circuit->GetSetupManager()->GetBasePos();
		}
		CTerrainManager* terrainMgr = circuit->GetTerrainManager();
		buildPos = terrainMgr->GetBuildPosition(bdef, buildPos);

		if (terrainMgr->CanBeBuiltAtSafe(airpadDef, buildPos) &&
			((unit == nullptr) || terrainMgr->CanReachAtSafe(unit, buildPos, unit->GetCircuitDef()->GetBuildDistance())))
		{
			return builderMgr->EnqueueFactory(IBuilderTask::Priority::NORMAL, airpadDef, buildPos);
		}
	}

	/*
	 * check buildpower
	 */
	const float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome())/* * ecoFactor*/;
	CCircuitDef* assistDef = factoryMgr->GetAssistDef();
	const float factoryFactor = (metalIncome - assistDef->GetBuildSpeed()) * 1.2f;
	const int nanoSize = builderMgr->GetTasks(IBuilderTask::BuildType::NANO).size();
	const float factoryPower = factoryMgr->GetFactoryPower() + nanoSize * assistDef->GetBuildSpeed();
	const bool isSwitchTime = (lastFacFrame + switchTime <= frame);
	if ((factoryPower >= factoryFactor) && !isSwitchTime) {
		return nullptr;
	}

	/*
	 * check nanos
	 */
	if (!isSwitchTime) {
		CCircuitUnit* factory = factoryMgr->NeedUpgrade();
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

			CTerrainManager* terrainMgr = circuit->GetTerrainManager();
			CCircuitDef* bdef = (unit == nullptr) ? factory->GetCircuitDef() : unit->GetCircuitDef();
			CTerrainManager::CorrectPosition(buildPos);
			buildPos = terrainMgr->GetBuildPosition(bdef, buildPos);

			if (terrainMgr->CanBeBuiltAtSafe(assistDef, buildPos) &&
				((unit == nullptr) || terrainMgr->CanReachAtSafe(unit, buildPos, unit->GetCircuitDef()->GetBuildDistance())))
			{
				return builderMgr->EnqueueTask(IBuilderTask::Priority::HIGH, assistDef, buildPos,
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

	const AIFloat3& enemyPos = circuit->GetEnemyManager()->GetEnemyPos();
	const bool isStart = (factoryMgr->GetFactoryCount() == 0);
	AIFloat3 buildPos;
	if (isStart) {
		buildPos = circuit->GetSetupManager()->GetBasePos();
	} else {
		AIFloat3 pos(circuit->GetSetupManager()->GetBasePos());
		CMetalManager* metalMgr = circuit->GetMetalManager();
		AIFloat3 center = (pos + enemyPos) * 0.5f;
		float minSqDist = std::numeric_limits<float>::max();
		const CMetalData::Clusters& clusters = metalMgr->GetClusters();
		for (unsigned i = 0; i < clusters.size(); ++i) {
			if (!metalMgr->IsClusterFinished(i)) {
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
		int index = metalMgr->FindNearestCluster(pos, predicate);
		if (index < 0) {
			return nullptr;
		}
		buildPos = clusters[index].position;
	}

	CCircuitDef* facDef = factoryMgr->GetFactoryToBuild(buildPos, isStart);
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
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CCircuitDef* bdef;
	CCircuitDef* landDef = factoryMgr->GetLandDef(facDef);
	if (landDef != nullptr) {
		if (landDef->GetMobileId() < 0) {
			bdef = landDef;
		} else {
			STerrainMapArea* area = terrainMgr->GetMobileTypeById(landDef->GetMobileId())->areaLargest;
			// FIXME: area->percentOfMap < 40.0 doesn't seem right as water identifier
			bdef = ((area == nullptr) || (area->percentOfMap < 40.0)) ? factoryMgr->GetWaterDef(facDef) : landDef;
		}
	} else {
		bdef = factoryMgr->GetWaterDef(facDef);
	}
	if (bdef == nullptr) {
		return nullptr;
	}

	CTerrainManager::CorrectPosition(buildPos);
	buildPos = terrainMgr->GetBuildPosition(bdef, buildPos);

	if (terrainMgr->CanBeBuiltAtSafe(facDef, buildPos) &&
		((unit == nullptr) || terrainMgr->CanReachAtSafe(unit, buildPos, unit->GetCircuitDef()->GetBuildDistance())))
	{
		lastFacFrame = frame;
		IBuilderTask::Priority priority = (builderMgr->GetWorkerCount() <= 2) ?
										  IBuilderTask::Priority::NOW :
										  IBuilderTask::Priority::HIGH;
		return builderMgr->EnqueueFactory(priority, facDef, buildPos);
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
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	if (!builderMgr->CanEnqueueTask()) {
		return nullptr;
	}

	const float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome());
	if ((storeDef == nullptr) ||
		!builderMgr->GetTasks(IBuilderTask::BuildType::STORE).empty() ||
		(GetStorage(metalRes) > 10 * metalIncome) ||
		!storeDef->IsAvailable(circuit->GetLastFrame()))
	{
		return UpdatePylonTasks();
	}

	const AIFloat3& startPos = circuit->GetSetupManager()->GetBasePos();
	CMetalManager* metalMgr = circuit->GetMetalManager();
	int index = metalMgr->FindNearestSpot(startPos);
	AIFloat3 buildPos;
	if (index != -1) {
		const CMetalData::Metals& spots = metalMgr->GetSpots();
		buildPos = spots[index].position;
	} else {
		CTerrainManager* terrainMgr = circuit->GetTerrainManager();
		int terWidth = terrainMgr->GetTerrainWidth();
		int terHeight = terrainMgr->GetTerrainHeight();
		float x = terWidth / 4 + rand() % (int)(terWidth / 2);
		float z = terHeight / 4 + rand() % (int)(terHeight / 2);
		buildPos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	}
	return builderMgr->EnqueueTask(IBuilderTask::Priority::HIGH, storeDef, buildPos, IBuilderTask::BuildType::STORE);
}

IBuilderTask* CEconomyManager::UpdatePylonTasks()
{
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	if (!builderMgr->CanEnqueueTask()) {
		return nullptr;
	}

	const float energyIncome = GetAvgEnergyIncome();
	const float metalIncome = std::min(GetAvgMetalIncome(), energyIncome);
	if (metalIncome < 30) {
		return nullptr;
	}

	const float cost = pylonDef->GetCostM();
	unsigned count = builderMgr->GetBuildPower() / cost * 8 + 1;
	if (builderMgr->GetTasks(IBuilderTask::BuildType::PYLON).size() >= count) {
		return nullptr;
	}

	energyGrid->SetAuthority(circuit);
	energyGrid->Update();

	CCircuitDef* buildDef;
	AIFloat3 buildPos;
	IGridLink* link = energyGrid->GetLinkToBuild(buildDef, buildPos);
	if ((link == nullptr) || (buildDef == nullptr)) {
		return nullptr;
	}

	if (utils::is_valid(buildPos)) {
		IBuilderTask::Priority priority = metalIncome < 40 ? IBuilderTask::Priority::NORMAL : IBuilderTask::Priority::HIGH;
		return builderMgr->EnqueuePylon(priority, buildDef, buildPos, link, buildDef->GetCostM());
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

void CEconomyManager::OpenStrategy(const CCircuitDef* facDef, const AIFloat3& pos)
{
	const std::vector<CCircuitDef::RoleT>* opener = circuit->GetSetupManager()->GetOpener(facDef);
	if (opener == nullptr) {
		return;
	}
	CFactoryManager* factoryMgr = circuit->GetFactoryManager();
	for (CCircuitDef::RoleT type : *opener) {
		CCircuitDef* buildDef = factoryMgr->GetRoleDef(facDef, type);
		if ((buildDef == nullptr) || !buildDef->IsAvailable(circuit->GetLastFrame())) {
			continue;
		}
		CRecruitTask::Priority priotiry;
		CRecruitTask::RecruitType recruit;
		if (type == ROLE_TYPE(BUILDER)) {
			priotiry = CRecruitTask::Priority::NORMAL;
			recruit  = CRecruitTask::RecruitType::BUILDPOWER;
		} else {
			priotiry = CRecruitTask::Priority::HIGH;
			recruit  = CRecruitTask::RecruitType::FIREPOWER;
		}
		factoryMgr->EnqueueTask(priotiry, buildDef, pos, recruit, 128.f);
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

	const float curMetal = GetMetalCur();
	const float storMetal = GetStorage(metalRes);
	isMetalEmpty = curMetal < storMetal * 0.2f;
	isMetalFull = curMetal > storMetal * 0.8f;
	isEnergyStalling = std::min(GetAvgMetalIncome() - GetMetalPull(), .0f)/* * 0.98f*/ > std::min(GetAvgEnergyIncome() - GetEnergyPull(), .0f);
	const float curEnergy = GetEnergyCur();
	const float storEnergy = GetStorage(energyRes);
	isEnergyEmpty = curEnergy < storEnergy * 0.1f;

	if (ecoFrame <= efInfo.startFrame) {
		energyFactor = efInfo.startFactor;
	} else if (ecoFrame >= efInfo.endFrame) {
		energyFactor = efInfo.endFactor;
	} else {
		energyFactor = efInfo.fraction * (ecoFrame - efInfo.startFrame) + efInfo.startFactor;
	}

	if (ecoFrame <= bdInfo.startFrame) {
		buildDelay = bdInfo.startDelay;
	} else if (ecoFrame >= bdInfo.endFrame) {
		buildDelay = bdInfo.endDelay;
	} else {
		buildDelay = int(bdInfo.fraction * (ecoFrame - bdInfo.startFrame)) + bdInfo.startDelay;
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
