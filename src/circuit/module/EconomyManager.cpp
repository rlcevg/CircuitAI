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
#include "scheduler/Scheduler.h"
#include "script/EconomyScript.h"
#include "setup/SetupManager.h"
#include "resource/MetalManager.h"
#include "resource/EnergyGrid.h"
#include "task/builder/FactoryTask.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
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
		, mexCount(0)
		, indexRes(0)
		, metalProduced(.0f)
		, metalUsed(.0f)
		, ecoFrame(-1)
		, isMetalEmpty(false)
		, isMetalFull(false)
		, isEnergyStalling(false)
		, isEnergyEmpty(false)
		, isEnergyRequired(false)
		, metal(SResourceInfo {-1, .0f, .0f, .0f, .0f})
		, energy(SResourceInfo {-1, .0f, .0f, .0f, .0f})
		, energyUse(.0f)
		, factoryTask(nullptr)
{
	metalRes = circuit->GetCallback()->GetResourceByName(RES_NAME_METAL);
	energyRes = circuit->GetCallback()->GetResourceByName(RES_NAME_ENERGY);
	economy = circuit->GetCallback()->GetEconomy();

	metalIncomes.resize(INCOME_SAMPLES, 1.0f);  // Init metal income
	energyIncomes.resize(INCOME_SAMPLES, 2.0f);  // Init energy income

	// TODO: Use A* ai planning... or sth... STRIPS https://ru.wikipedia.org/wiki/STRIPS
	//       https://ru.wikipedia.org/wiki/Марковский_процесс_принятия_решений

	circuit->GetScheduler()->RunOnInit(CScheduler::GameJob(&CEconomyManager::Init, this));

	/*
	 * factory handlers
	 */
	auto factoryFinishedHandler = [this](CCircuitUnit* unit) {
		const int frame = this->circuit->GetLastFrame();
		const int index = this->circuit->GetMetalManager()->FindNearestCluster(unit->GetPos(frame));
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
		auto it = std::find(energyDefs.infos.begin(), energyDefs.infos.end(), unit->GetCircuitDef());
		if (it != energyDefs.infos.end()) {
			const float income = it->data.make;
			for (int i = 0; i < INCOME_SAMPLES; ++i) {
				energyIncomes[i] += income;
			}
			energy.income += income;
		}
	};
	auto mexFinishedHandler = [this](CCircuitUnit* unit) {
//		const float income = unit->GetUnit()->GetRulesParamFloat("mexIncome", 0.f);
		CMetalManager* metalMgr = this->circuit->GetMetalManager();
		int index = metalMgr->FindNearestSpot(unit->GetPos(this->circuit->GetLastFrame()));
		if (index < 0) {
			return;
		}
		const float income = metalMgr->GetSpots()[index].income;
		for (int i = 0; i < INCOME_SAMPLES; ++i) {
			metalIncomes[i] += income;
		}
		metal.income += income;
	};

	/*
	 * morph & plop
	 */
	auto comFinishedHandler = [this](CCircuitUnit* unit) {
		AddMorphee(unit);
		CSetupManager* setupMgr = this->circuit->GetSetupManager();
		if (setupMgr->GetCommander() == nullptr) {
			setupMgr->SetCommander(unit);
		}

		ICoreUnit::Id unitId = unit->GetId();
		this->circuit->GetScheduler()->RunJobAfter(CScheduler::GameJob([this, unitId]() {
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
					CTerrainManager* terrainMgr = this->circuit->GetTerrainManager();
					buildPos = terrainMgr->GetBuildPosition(facDef, pos);
					CBuilderManager* builderMgr = this->circuit->GetBuilderManager();
					IBuilderTask* task = builderMgr->EnqueueFactory(IBuilderTask::Priority::NOW, facDef, nullptr, buildPos,
																	SQUARE_SIZE, true, true, 0);
					static_cast<ITaskManager*>(builderMgr)->AssignTask(unit, task);
				}
			}

			if (!isStart) {
				return;
			}
			int morphFrame = this->circuit->GetSetupManager()->GetMorphFrame(unit->GetCircuitDef());
			if (morphFrame >= 0) {
				this->circuit->GetScheduler()->RunJobAt(CScheduler::GameJob([this, unitId]() {
					// Force commander level 0 to morph
					CCircuitUnit* unit = this->circuit->GetTeamUnit(unitId);
					if ((unit != nullptr)
						&& (unit->GetTask()->GetType() != IUnitTask::Type::PLAYER))
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

	ReadConfig();

	float maxAreaDivCost = .0f;
	const float avgWind = (circuit->GetMap()->GetMaxWind() + circuit->GetMap()->GetMinWind()) * 0.5f;

	for (CCircuitDef& cdef : circuit->GetCircuitDefs()) {
		const std::map<std::string, std::string>& customParams = cdef.GetDef()->GetCustomParams();

		if (!cdef.IsMobile()) {
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
			if (cdef.GetDef()->GetStorage(metalRes) >= 1000.f) {
				storeMDefs.all.insert(&cdef);
			}
			if (cdef.GetDef()->GetStorage(energyRes) > 1000.f) {
				storeEDefs.all.insert(&cdef);
			}

			// mex
			// BA: float metalConverts = unitDef->GetMakesResource(metalRes);
			//     float metalExtracts = unitDef->GetExtractsResource(metalRes);
			//     float netMetal = unitDef->GetResourceMake(metalRes) - unitDef->GetUpkeep(metalRes);
			// FIXME: BA
//			if (((it = customParams.find("ismex")) != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
//				finishedHandler[cdef.GetId()] = mexFinishedHandler;
//				mexDef = &cdef;  // cormex
//				cdef.SetIsMex(true);
//			}
			// FIXME: BA

			// factory and assist
			if (cdef.GetDef()->IsBuilder()) {
				if (!cdef.GetBuildOptions().empty()) {
					finishedHandler[cdef.GetId()] = factoryFinishedHandler;
					destroyedHandler[cdef.GetId()] = factoryDestroyedHandler;
				} else if (cdef.IsAbleToAssist()
					&& (std::max(cdef.GetDef()->GetXSize(), cdef.GetDef()->GetZSize()) * SQUARE_SIZE < cdef.GetBuildDistance()))
				{
					assistDefs.all.insert(&cdef);
					cdef.SetIsAssist(true);
				}
			}

			// energy
			// BA: float netEnergy = unitDef->GetResourceMake(energyRes) - unitDef->GetUpkeep(energyRes);
			it = customParams.find("income_energy");
			if (((it != customParams.end()) && (utils::string_to_float(it->second) > 1))
				|| (cdef.GetDef()->GetResourceMake(energyRes) - cdef.GetUpkeepE() > 1)
				|| ((cdef.GetDef()->GetWindResourceGenerator(energyRes) > 5) && (avgWind > 5))
				|| (cdef.GetDef()->GetTidalResourceGenerator(energyRes) * circuit->GetMap()->GetTidalStrength() > 1))
			{
				finishedHandler[cdef.GetId()] = energyFinishedHandler;
				energyDefs.all.insert(&cdef);
			}

		} else {

			// commander
			if (cdef.IsRoleComm()) {
				finishedHandler[cdef.GetId()] = comFinishedHandler;
				destroyedHandler[cdef.GetId()] = comDestroyedHandler;
			}

			for (const SSideInfo& sideInfo : sideInfos) {
				if (cdef.CanBuild(sideInfo.mexDef)) {
					mexDefs[cdef.GetId()] = sideInfo.mexDef;
				}
				if (cdef.CanBuild(sideInfo.defaultDef)) {
					defaultDefs[cdef.GetId()] = sideInfo.defaultDef;
				}
			}
		}
	}

	// FIXME: BA
	for (SSideInfo& sideInfo : sideInfos) {
		CCircuitDef*& mexDef = sideInfo.mexDef;
		if (mexDef != nullptr) {
			finishedHandler[mexDef->GetId()] = mexFinishedHandler;
			mexDef->SetIsMex(true);
		} else {
			mexDef = sideInfo.defaultDef;
		}
	}
	if (pylonDef == nullptr) {
		pylonDef = sideInfos[0].defaultDef;
		pylonRange = PYLON_RANGE;
	}
	// FIXME: BA
}

CEconomyManager::~CEconomyManager()
{
	delete metalRes;
	delete energyRes;
	delete economy;

	delete factoryTask;
}

void CEconomyManager::ReadConfig()
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();
	const Json::Value& econ = root["economy"];
	ecoStep = econ.get("eps_step", 0.25f).asFloat();
	ecoFactor = (circuit->GetAllyTeam()->GetSize() - 1.0f) * ecoStep + 1.0f;
	metalMod = (1.f - econ.get("excess", -1.f).asFloat());
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

	costRatio = energy.get("cost_ratio", 0.05f).asFloat();
	clusterRange = econ.get("cluster_range", 950.f).asFloat();

	CMaskHandler& sideMasker = circuit->GetGameAttribute()->GetSideMasker();
	sideInfos.resize(sideMasker.GetMasks().size());
	const Json::Value& engySides = energy["side"];
	const Json::Value& metal = econ["metal"];
	const Json::Value& deflt = econ["default"];
	for (const auto& kv : sideMasker.GetMasks()) {
		SSideInfo& sideInfo = sideInfos[kv.second.type];
		const Json::Value& surfs = engySides[kv.first];

		std::vector<std::pair<std::string, SEnergyCond>> engies;
		std::string type = circuit->GetTerrainManager()->IsWaterMap() ? "water" : "land";
		const Json::Value& surf = surfs[type];
		for (const std::string& engy : surf.getMemberNames()) {
			const Json::Value& surfEngy = surf[engy];
			const int min = surfEngy[0].asInt();
			const int max = surfEngy.get(1, min).asInt();
			SEnergyCond cond;
			cond.limit = min + rand() % (max - min + 1);
			cond.metalIncome = surfEngy.get(2, -1.f).asFloat();
			cond.energyIncome = surfEngy.get(3, -1.f).asFloat();
			cond.score = surfEngy.get(4, -1.f).asFloat();
			engies.push_back(std::make_pair(engy, cond));
		}

		std::unordered_map<CCircuitDef*, SEnergyCond>& list = sideInfo.engyLimits;
		for (unsigned i = 0; i < engies.size(); ++i) {
			const char* name = engies[i].first.c_str();
			CCircuitDef* cdef = circuit->GetCircuitDef(name);
			if (cdef == nullptr) {
				circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), name);
				continue;
			}
			list[cdef] = engies[i].second;
		}

		// Metal
		const char* name = metal[kv.first][0].asCString();
		CCircuitDef* cdef = circuit->GetCircuitDef(name);
		if (cdef != nullptr) {
			sideInfo.mexDef = cdef;
		} else {
			circuit->LOG("CONFIG %s: has unknown mexDef '%s'", cfgName.c_str(), name);
		}
		name = metal[kv.first][1].asCString();
		cdef = circuit->GetCircuitDef(name);
		if (cdef != nullptr) {
			sideInfo.mohoMexDef = cdef;
		} else {
			circuit->LOG("CONFIG %s: has unknown mohoMexDef '%s'", cfgName.c_str(), name);
		}

		// Default
		// NOTE: Must have
		sideInfo.defaultDef = circuit->GetCircuitDef(deflt[kv.first].asCString());
		if (sideInfo.defaultDef == nullptr) {
			throw CException("economy.default");
		}
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
		metalProduced = GetMetalCur() * metalMod;

		CScheduler* scheduler = circuit->GetScheduler().get();
		CAllyTeam* allyTeam = circuit->GetAllyTeam();
		if (circuit->IsCommMerge() && !circuit->IsLoadSave()) {
			const int spotId = circuit->GetMetalManager()->FindNearestSpot(pos);
			const int clusterId = (spotId < 0) ? -1 : circuit->GetMetalManager()->GetCluster(spotId);
			int ownerId = allyTeam->GetClusterTeam(clusterId).teamId;
			if (ownerId < 0) {
				allyTeam->OccupyCluster(clusterId, circuit->GetTeamId());
			} else if (ownerId != circuit->GetTeamId()) {
				circuit->Resign(ownerId);
				return;
			}

			CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
			if (commander == nullptr) {
				commander = circuit->GetTeamUnits().begin()->second;
			}
			ownerId = allyTeam->GetAreaTeam(commander->GetArea()).teamId;
			if (ownerId < 0) {
				allyTeam->OccupyArea(commander->GetArea(), circuit->GetTeamId());
//			} else if (ownerId != circuit->GetTeamId()) {
//				circuit->Resign(ownerId);
//				return;
			}
		}

		scheduler->RunJobAfter(CScheduler::GameJob([this]() {
			ecoFactor = (circuit->GetAllyTeam()->GetAliveSize() - 1.0f) * ecoStep + 1.0f;
		}), FRAMES_PER_SEC * 10);

		const float maxTravel = 7 + rand() % (10 - 7 + 1);  // seconds
		const int interval = allyTeam->GetSize() * FRAMES_PER_SEC;
		startFactory = CScheduler::GameJob(&CEconomyManager::StartFactoryTask, this, maxTravel);
		scheduler->RunJobEvery(startFactory, 1, circuit->GetSkirmishAIId() + 0 + 5 * FRAMES_PER_SEC);
		scheduler->RunJobEvery(CScheduler::GameJob(&CEconomyManager::UpdateStorageTasks, this),
								interval, circuit->GetSkirmishAIId() + 1 + interval / 2);

		scheduler->RunJobEvery(CScheduler::GameJob(&CEconomyManager::UpdateResourceIncome, this), TEAM_SLOWUPDATE_RATE);
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

CCircuitDef* CEconomyManager::GetLowEnergy(const AIFloat3& pos, float& outMake, CCircuitUnit* builder) const
{
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CCircuitDef* result = nullptr;
	const int frame = circuit->GetLastFrame();
	auto it = energyDefs.infos.rbegin();
	while (it != energyDefs.infos.rend()) {
		CCircuitDef* candy = it->cdef;
		if (candy->IsAvailable(frame)
			&& ((builder == nullptr) || builder->GetCircuitDef()->CanBuild(candy))
			&& terrainMgr->CanBeBuiltAtSafe(candy, pos))
		{
			result = candy;
			outMake = it->data.make;
			break;
		}
		++it;
	}
	return result;
}

void CEconomyManager::AddEconomyDefs(const std::set<CCircuitDef*>& buildDefs)
{
	const std::unordered_map<CCircuitDef*, SEnergyCond>& list = GetSideInfo().engyLimits;
	energyDefs.AddDefs(buildDefs, [this, &list](CCircuitDef* cdef, SEnergyInfo& data) -> float {
		auto customParams = cdef->GetDef()->GetCustomParams();
		auto it = customParams.find("income_energy");
		data.make = (it != customParams.end())
				? utils::string_to_float(it->second)
				: cdef->GetDef()->GetResourceMake(energyRes) - cdef->GetUpkeepE() - cdef->GetCloakCost();
		if (data.make < 1) {
			data.make = cdef->GetDef()->GetWindResourceGenerator(energyRes);
			if (data.make < 1) {
				data.make = cdef->GetDef()->GetTidalResourceGenerator(energyRes) * circuit->GetMap()->GetTidalStrength();
			} else {
				float avgWind = (circuit->GetMap()->GetMaxWind() + circuit->GetMap()->GetMinWind()) * 0.5f;
				data.make = std::min(avgWind, cdef->GetDef()->GetWindResourceGenerator(energyRes));
			}
		}

		auto lit = list.find(cdef);
		if (lit != list.end()) {
			data.cond = lit->second;
		}
		if (data.cond.score < .0f) {
			// TODO: Instead of plain sizeX, sizeZ use AI's yardmap size
			data.cond.score = SQUARE(data.make) / ((cdef->GetCostM()/* + cdef->GetCostE() * 0.05f*/) * cdef->GetDef()->GetXSize() * cdef->GetDef()->GetZSize());
		}
		if (data.cond.metalIncome < 0.f) {
			// TODO: Select proper scale/quadratic function (x*x) and smoothing coefficient (8).
			//       МЕТОД НАИМЕНЬШИХ КВАДРАТОВ ! (income|buildPower, make/cost) - points
			//       solar       geothermal    fusion         singu           ...
			//       (10, 2/70), (15, 25/500), (20, 35/1000), (30, 225/4000), ...
			data.cond.metalIncome = sqrtf(cdef->GetCostM() * 16.0f / MAX_BUILD_SEC);
		}
		if (data.cond.energyIncome < 0.f) {
			data.cond.energyIncome = cdef->GetCostE() * costRatio;
		}

		return data.cond.score;
	});

	auto scoreFunc = [](CCircuitDef* cdef, const SStoreInfo& data) {
		return data.storage / cdef->GetCostM();
	};
	storeMDefs.AddDefs(buildDefs, [this, scoreFunc](CCircuitDef* cdef, SStoreInfo& data) -> float {
		data.storage = cdef->GetDef()->GetStorage(metalRes);
		return scoreFunc(cdef, data);
	});
	storeEDefs.AddDefs(buildDefs, [this, scoreFunc](CCircuitDef* cdef, SStoreInfo& data) -> float {
		data.storage = cdef->GetDef()->GetStorage(energyRes);
		return scoreFunc(cdef, data);
	});

	assistDefs.AddDefs(buildDefs, [](CCircuitDef* cdef, SAssistInfo& data) -> float {
		return cdef->GetBuildSpeed() / cdef->GetCostM();
	});

	// DEBUG
//	circuit->LOG("----Energy----");
//	for (const auto& ei : energyDefs.infos) {
//		circuit->LOG("%s | costM=%f | costE=%f | make=%f | efficiency=%f | limit=%i | m-income=%f | e-income=%f", ei.cdef->GetDef()->GetName(),
//				ei.cdef->GetCostM(), ei.cdef->GetCostE(), ei.data.make, ei.data.cond.score, ei.data.cond.limit, ei.data.cond.metalIncome, ei.data.cond.energyIncome);
//	}
//	std::vector<std::pair<std::string, CAvailList<SStoreInfo>*>> vec = {{"Metal", &storeMDefs}, {"Energy", &storeEDefs}};
//	for (const auto& kv : vec) {
//		circuit->LOG("----%s Storage----", kv.first.c_str());
//		for (const auto& si : kv.second->infos) {
//			circuit->LOG("%s | costM=%f | costE=%f | storage=%f", si.cdef->GetDef()->GetName(),
//					si.cdef->GetCostM(), si.cdef->GetCostE(), si.data.storage);
//		}
//	}
//	circuit->LOG("----Assist----");
//	for (const auto& ni : assistDefs.infos) {
//		circuit->LOG("%s | costM=%f | costE=%f | build_speed=%f | efficiency=%f", ni.cdef->GetDef()->GetName(),
//				ni.cdef->GetCostM(), ni.cdef->GetCostE(), ni.cdef->GetBuildSpeed(), ni.score);
//	}
}

void CEconomyManager::RemoveEconomyDefs(const std::set<CCircuitDef*>& buildDefs)
{
	energyDefs.RemoveDefs(buildDefs);
	storeMDefs.RemoveDefs(buildDefs);
	storeEDefs.RemoveDefs(buildDefs);
	assistDefs.RemoveDefs(buildDefs);

	// DEBUG
//	circuit->LOG("----Remove Energy----");
//	for (const auto& ei : energyDefs.infos) {
//		circuit->LOG("%s | costM=%f | costE=%f | make=%f | efficiency=%f | limit=%i", ei.cdef->GetDef()->GetName(),
//				ei.cdef->GetCostM(), ei.cdef->GetCostE(), ei.data.make, ei.data.cond.score, ei.data.cond.limit);
//	}
//	circuit->LOG("----Remove Metal Storage----");
//	for (const auto& si : storeMDefs.infos) {
//		circuit->LOG("%s | costM=%f | costE=%f | storage=%f", si.cdef->GetDef()->GetName(),
//				si.cdef->GetCostM(), si.cdef->GetCostE(), si.data.storage);
//	}
//	circuit->LOG("----Remove Energy Storage----");
//	for (const auto& si : storeEDefs.infos) {
//		circuit->LOG("%s | costM=%f | costE=%f | storage=%f", si.cdef->GetDef()->GetName(),
//				si.cdef->GetCostM(), si.cdef->GetCostE(), si.data.storage);
//	}
//	circuit->LOG("----Remove Assist----");
//	for (const auto& ni : assistDefs.infos) {
//		circuit->LOG("%s | costM=%f | costE=%f | build_speed=%f | efficiency=%f", ni.cdef->GetDef()->GetName(),
//				ni.cdef->GetCostM(), ni.cdef->GetCostE(), ni.cdef->GetBuildSpeed(), ni.score);
//	}
}

const CEconomyManager::SSideInfo& CEconomyManager::GetSideInfo() const
{
	return sideInfos[circuit->GetSideId()];
}

void CEconomyManager::UpdateResourceIncome()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	float oddEnergyIncome = circuit->GetTeam()->GetRulesParamFloat("OD_energyIncome", 0.f);
	float oddEnergyChange = circuit->GetTeam()->GetRulesParamFloat("OD_energyChange", 0.f);

	energyIncomes[indexRes] = economy->GetIncome(energyRes) + oddEnergyIncome - std::max(.0f, oddEnergyChange);
	metalIncomes[indexRes] = economy->GetIncome(metalRes) + economy->GetReceived(metalRes);
	++indexRes %= INCOME_SAMPLES;

	metal.income = .0f;
	for (int i = 0; i < INCOME_SAMPLES; i++) {
		metal.income += metalIncomes[i];
	}
	metal.income /= INCOME_SAMPLES;

	energy.income = .0f;
	for (int i = 0; i < INCOME_SAMPLES; i++) {
		energy.income += energyIncomes[i];
	}
	energy.income /= INCOME_SAMPLES;

	metalProduced += metal.income * metalMod;
	metalUsed += economy->GetUsage(metalRes);
}

float CEconomyManager::GetMetalCur()
{
	return metal.current = economy->GetCurrent(metalRes);
}

float CEconomyManager::GetMetalStore()
{
	return metal.storage = GetStorage(metalRes);
}

float CEconomyManager::GetMetalPull()
{
	if (metal.pullFrame/* + TEAM_SLOWUPDATE_RATE*/ < circuit->GetLastFrame()) {
		metal.pullFrame = circuit->GetLastFrame();
		metal.pull = economy->GetPull(metalRes) + circuit->GetTeam()->GetRulesParamFloat("extraMetalPull", 0.f);
	}
	return metal.pull;
}

float CEconomyManager::GetEnergyCur()
{
	return energy.current = economy->GetCurrent(energyRes);
}

float CEconomyManager::GetEnergyStore()
{
	return energy.storage = GetStorage(energyRes);
}

float CEconomyManager::GetEnergyPull()
{
	if (energy.pullFrame/* + TEAM_SLOWUPDATE_RATE*/ < circuit->GetLastFrame()) {
		energy.pullFrame = circuit->GetLastFrame();
		float extraEnergyPull = circuit->GetTeam()->GetRulesParamFloat("extraEnergyPull", 0.f);
//		float oddEnergyOverdrive = circuit->GetTeam()->GetRulesParamFloat("OD_energyOverdrive", 0.f);
//		float oddEnergyChange = circuit->GetTeam()->GetRulesParamFloat("OD_energyChange", 0.f);
//		float extraChange = std::min(.0f, oddEnergyChange) - std::min(.0f, oddEnergyOverdrive);
//		float teamEnergyWaste = circuit->GetTeam()->GetRulesParamFloat("OD_team_energyWaste", 0.f);
//		float numAllies = circuit->GetTeam()->GetRulesParamFloat("OD_allies", 1.f);
//		if (numAllies < 1.f) {
//			numAllies = 1.f;
//		}
		energy.pull = economy->GetPull(energyRes) + extraEnergyPull/* + extraChange - teamEnergyWaste / numAllies*/;
	}
	return energy.pull;
}

float CEconomyManager::GetEnergyUse()
{
	return energyUse = economy->GetUsage(energyRes);
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
	CCircuitDef* mexDef = GetMexDef(unit->GetCircuitDef());

	// check uncolonized mexes
	bool isEnergyStalling = IsEnergyStalling();
	if ((mexDef != nullptr) && !isEnergyStalling && mexDef->IsAvailable(circuit->GetLastFrame())) {
		float cost = mexDef->GetCostM();
		unsigned maxCount = builderMgr->GetBuildPower() / cost * 8 + 3;
		if (builderMgr->GetTasks(IBuilderTask::BuildType::MEX).size() < maxCount) {
			CMetalManager* metalMgr = circuit->GetMetalManager();
			CTerrainManager* terrainMgr = circuit->GetTerrainManager();
			const CMetalData::Metals& spots = metalMgr->GetSpots();
			CMap* map = circuit->GetMap();
			CMetalData::PointPredicate predicate;
			if (unit != nullptr) {
				predicate = [this, &spots, map, mexDef, terrainMgr, unit](int index) {
					return (IsAllyOpenSpot(index)
							&& terrainMgr->CanBeBuiltAtSafe(mexDef, spots[index].position)  // hostile environment
							&& terrainMgr->CanReachAtSafe(unit, spots[index].position, unit->GetCircuitDef()->GetBuildDistance())
							&& map->IsPossibleToBuildAt(mexDef->GetDef(), spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
				};
			} else {
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

	bool isResurrect = unit->GetCircuitDef()->IsAbleToResurrect();  // false;
	if (IsMetalFull() || (builderMgr->GetTasks(IBuilderTask::BuildType::RECLAIM).size() >= builderMgr->GetWorkerCount() / 2)) {
//		isResurrect = unit->GetCircuitDef()->IsAbleToResurrect();
		if (!isResurrect) {
			return nullptr;
		}
	}

	std::vector<Feature*> features;
	if (isNear) {
		const float distance = unit->GetCircuitDef()->GetSpeed() * ((GetMetalPull() * 0.8f > GetAvgMetalIncome()) ? 60 : 20);
		features = std::move(circuit->GetCallback()->GetFeaturesIn(position, distance));
	} else {
		features = std::move(circuit->GetCallback()->GetFeatures());
	}
	if (features.empty()) {
		return nullptr;
	}

	COOAICallback* clb = circuit->GetCallback();
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
		FeatureDef* featDef;
		if (isResurrect) {
			if (!clb->Feature_IsResurrectable(feature->GetFeatureId())) {
				continue;
			}
			featDef = feature->GetDef();
		} else {
			featDef = feature->GetDef();
			if (!featDef->IsReclaimable()) {
				delete featDef;
				continue;
			}
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
	utils::free_clear(features);

	IBuilderTask* task = nullptr;
	if (minSqDist < std::numeric_limits<float>::max()) {
		task = builderMgr->GetReclaimFeatureTask(pos, 8.0f/*unit->GetCircuitDef()->GetBuildDistance()*/);
		if (task != nullptr) {
			return task;
		}
		task = builderMgr->GetResurrectTask(pos, 8.0f/*unit->GetCircuitDef()->GetBuildDistance()*/);
		if (isResurrect) {
			if (task == nullptr) {
				task = builderMgr->EnqueueResurrect(IBuilderTask::Priority::HIGH, pos, cost, FRAMES_PER_SEC * 300,
													8.0f/*unit->GetCircuitDef()->GetBuildDistance()*/);
			}
		} else if (task == nullptr) {
			task = builderMgr->EnqueueReclaim(IBuilderTask::Priority::HIGH, pos, cost, FRAMES_PER_SEC * 300,
											  8.0f/*unit->GetCircuitDef()->GetBuildDistance()*/);
		} else {
			task = nullptr;
		}
	}

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
	bool isEnergyStalling = IsEnergyStalling();
	// TODO: e-stalling needs separate array of energy-defs sorted by cost

	// Select proper energy UnitDef to build
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CCircuitDef* bestDef = nullptr;
	CCircuitDef* hopeDef = nullptr;
	metalIncome = std::min(metalIncome, energyIncome) * energyFactor;
	const float buildPower = std::min(builderMgr->GetBuildPower(), metalIncome);
	const int taskSize = builderMgr->GetTasks(IBuilderTask::BuildType::ENERGY).size();
	const float buildTimeMod = isEnergyStalling ? 0.25f : 1.f;
	isEnergyStalling |= isEnergyRequired;
	bool isLastHope = isEnergyStalling;

	const int frame = circuit->GetLastFrame();
	for (const auto& engy : energyDefs.infos) {  // sorted by high-tech first
		// TODO: Add geothermal powerplant support
		if (!engy.cdef->IsAvailable(frame)
			|| !terrainMgr->CanBeBuiltAtSafe(engy.cdef, position)
			|| engy.cdef->GetDef()->IsNeedGeo())
		{
			continue;
		}

		if (engy.cdef->GetCount() < engy.data.cond.limit) {
			isLastHope = false;
			if (taskSize < (int)(buildPower / engy.cdef->GetCostM() * 4 + 1)) {
				bestDef = engy.cdef;
				if ((engy.data.cond.metalIncome < buildTimeMod * metalIncome)
					&& (engy.data.cond.energyIncome < energyIncome))
				{
					break;
				}
			} else if ((engy.data.cond.metalIncome < buildTimeMod * metalIncome)
				&& (engy.data.cond.energyIncome < energyIncome))
			{
				bestDef = nullptr;
				break;
			}
		} else if (!isEnergyStalling) {
			bestDef = nullptr;
			break;
		} else if (hopeDef == nullptr) {
			hopeDef = engy.cdef;
			isLastHope = isLastHope && (taskSize < (int)(buildPower / engy.cdef->GetCostM() * 4 + 1));
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
	if (bestDef->GetCostM() < 200.0f) {
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
			buildPos += (buildPos - mapCenter).Normalize2D() * 300.0f * (bestDef->GetCostM() / energyDefs.infos.front().cdef->GetCostM());
			CCircuitDef* bdef = (unit == nullptr) ? bestDef : unit->GetCircuitDef();
			CTerrainManager::CorrectPosition(buildPos);
			buildPos = circuit->GetTerrainManager()->GetBuildPosition(bdef, buildPos);
		}
	}

	if (utils::is_valid(buildPos) && terrainMgr->CanBeBuiltAtSafe(bestDef, buildPos) &&
		((unit == nullptr) || terrainMgr->CanReachAtSafe(unit, buildPos, unit->GetCircuitDef()->GetBuildDistance())))
	{
		IBuilderTask::Priority priority = isEnergyStalling
				? (IsEnergyEmpty() ? IBuilderTask::Priority::NOW : IBuilderTask::Priority::HIGH)
				: IBuilderTask::Priority::NORMAL;
		IBuilderTask* task = builderMgr->EnqueueTask(priority, bestDef, buildPos, IBuilderTask::BuildType::ENERGY, SQUARE_SIZE * 16.0f, true);
		if ((unit == nullptr) || unit->GetCircuitDef()->CanBuild(bestDef)) {
			return task;
		}
	}

	return nullptr;
}

IBuilderTask* CEconomyManager::UpdateFactoryTasks(const AIFloat3& position, CCircuitUnit* unit)
{
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	if (!builderMgr->CanEnqueueTask(64) || isEnergyRequired) {
		return nullptr;
	}

	/*
	 * check air pads
	 */
	const int frame = circuit->GetLastFrame();
	const std::set<IBuilderTask*> &factoryTasks = builderMgr->GetTasks(IBuilderTask::BuildType::FACTORY);
	CFactoryManager* factoryMgr = circuit->GetFactoryManager();
	CCircuitDef* airpadDef = (unit == nullptr) ? factoryMgr->GetSideInfo().airpadDef : factoryMgr->GetAirpadDef(unit->GetCircuitDef());
	if ((airpadDef != nullptr) && airpadDef->IsAvailable(frame)) {
		CMilitaryManager* militaryMgr = circuit->GetMilitaryManager();
		const unsigned airpadFactor = SQUARE((airpadDef->GetCount() + factoryTasks.size() + 1) * 3);
		if (militaryMgr->GetRoleUnits(ROLE_TYPE(AIR)).size() > airpadFactor) {
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
				return builderMgr->EnqueueFactory(IBuilderTask::Priority::NORMAL, airpadDef, nullptr, buildPos);
			}
		}
	}

	/*
	 * check assist
	 */
	const bool isSwitchTime = factoryMgr->IsSwitchTime();
	if (!isSwitchTime) {
		IBuilderTask* task = nullptr;
		if (CheckAssistRequired(position, unit, task)) {
			return task;
		}
	}

	/*
	 * check factory
	 */
	if (!factoryTasks.empty()) {
		return nullptr;
	}

	const bool isStart = (factoryMgr->GetFactoryCount() == 0);
	CCircuitDef* facDef;
	CCircuitDef* reprDef;
	if (factoryTask == nullptr) {
		facDef = factoryMgr->GetFactoryToBuild(-RgtVector, isStart);
		if (facDef == nullptr) {
			return nullptr;
		}
		reprDef = factoryMgr->GetRepresenter(facDef);
		if (reprDef == nullptr) {  // identify area to build by factory representatives
			return nullptr;
		}
		IBuilderTask::Priority priority = (builderMgr->GetWorkerCount() <= 2) ?
										  IBuilderTask::Priority::NOW :
										  IBuilderTask::Priority::HIGH;
		const bool isPlop = (factoryMgr->GetFactoryCount() <= 0);
		// hold selected facDef - create non-active task
		factoryTask = static_cast<CBFactoryTask*>(builderMgr->EnqueueFactory(priority,
				facDef, reprDef, -RgtVector, SQUARE_SIZE, isPlop, false, FRAMES_PER_SEC * 120));
	} else {
		facDef = factoryTask->GetBuildDef();
		reprDef = factoryTask->GetReprDef();
	}

	/*
	 * check metal and energy levels
	 */
	const float buildTime = reprDef->GetBuildTime() / facDef->GetWorkerTime();
	// resources required for factory to work
	const float miRequired = reprDef->GetCostM() / buildTime;
	const float eiRequired = reprDef->GetCostE() / buildTime;
//	const float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome());  // FIXME: ZK
	float engyFactor = (GetAvgEnergyIncome() - eiRequired) * 0.08f;
	float metalFactor = GetAvgMetalIncome() - miRequired;
	if (factoryMgr->GetFactoryCount() > 0) {
		engyFactor -= builderMgr->GetBuildPower() * 0.2f;
		metalFactor -= builderMgr->GetBuildPower() * 0.1f;
	}
	const int nanoSize = builderMgr->GetTasks(IBuilderTask::BuildType::NANO).size();
	const float factoryPower = /*factoryMgr->GetFactoryPower() + */nanoSize * factoryMgr->GetAssistSpeed();
	// FIXME: DEBUG
	circuit->LOG("%s | %f >= %f | %f >= %f", facDef->GetDef()->GetName(), metalFactor, factoryPower, engyFactor, factoryPower);
	circuit->LOG("%s | mi: %f | ei: %f | mr: %f | er: %f", reprDef->GetDef()->GetName(), GetAvgMetalIncome(), GetAvgEnergyIncome(), miRequired, eiRequired);
	// FIXME: DEBUG
	if ((metalFactor < factoryPower) && !isSwitchTime && (facDef->GetCostM() > GetMetalCur())) {
		return nullptr;
	}
	if (engyFactor < factoryPower) {
		isEnergyRequired = true;  // enough metal, request energy
		return nullptr;
	}

	// desired position
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	const AIFloat3& enemyPos = circuit->GetEnemyManager()->GetEnemyPos();
	AIFloat3 buildPos;
	if (factoryTask->IsPlop()) {
		buildPos = -RgtVector;
	} else {
		if (isStart || facDef->IsRoleSupport()) {
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

			CMetalData::PointPredicate predicate = [this, facDef, terrainMgr, &clusters](const int index) {
				return (clusterInfos[index].factory == nullptr) && terrainMgr->CanBeBuiltAtSafe(facDef, clusters[index].position);
			};
			int index = metalMgr->FindNearestCluster(pos, predicate);
			if (index < 0) {
				return nullptr;
			}
			buildPos = clusters[index].position;
		}

		const float sqStartDist = enemyPos.SqDistance2D(circuit->GetSetupManager()->GetStartPos());
		const float sqBuildDist = enemyPos.SqDistance2D(buildPos);
		float size = (sqStartDist > sqBuildDist) ? -200.0f : 200.0f;  // std::max(facUDef->GetXSize(), facUDef->GetZSize()) * SQUARE_SIZE;
		buildPos.x += (buildPos.x > enemyPos.x) ? -size : size;
		buildPos.z += (buildPos.z > enemyPos.z) ? -size : size;

		CTerrainManager::CorrectPosition(buildPos);
		buildPos = terrainMgr->GetBuildPosition(reprDef, buildPos);
	}

	factoryTask->SetPosition(buildPos);

	if (factoryTask->IsPlop()
		|| (terrainMgr->CanBeBuiltAtSafe(facDef, buildPos)
			&& ((unit == nullptr) || terrainMgr->CanReachAtSafe(unit, buildPos, unit->GetCircuitDef()->GetBuildDistance()))))
	{
		builderMgr->ActivateTask(factoryTask);
		IBuilderTask* task = factoryTask;
		factoryTask = nullptr;
		return task;
	} else {
		delete factoryTask;
		factoryTask = nullptr;
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
	if (!builderMgr->CanEnqueueTask(32)) {
		return nullptr;
	}

	if (!builderMgr->GetTasks(IBuilderTask::BuildType::STORE).empty()) {
		return UpdatePylonTasks();
	}

	CCircuitDef* storeDef = nullptr;
	if (!storeMDefs.infos.empty()) {
		storeDef = storeMDefs.infos.front().cdef;

		if (!storeDef->IsAvailable(circuit->GetLastFrame())
//			|| (GetMetalStore() > 60 * GetAvgMetalIncome())
			|| !IsMetalFull())
		{
			storeDef = nullptr;
		}
	}

	if ((storeDef == nullptr) && !storeEDefs.infos.empty()) {
		storeDef = storeEDefs.infos.front().cdef;

		if (!storeDef->IsAvailable(circuit->GetLastFrame())
			|| (GetEnergyStore() > 5 * GetAvgEnergyIncome())
			|| IsEnergyEmpty())
		{
			storeDef = nullptr;
		}
	}

	if (storeDef != nullptr) {
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

	return UpdatePylonTasks();
}

IBuilderTask* CEconomyManager::UpdatePylonTasks()
{
	return nullptr;
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
		circuit->GetScheduler()->RunJobAfter(CScheduler::GameJob([link](CEnergyGrid* energyGrid) {
			link->SetValid(true);
			energyGrid->SetForceRebuild(true);
		}, energyGrid), FRAMES_PER_SEC * 120);
	}

	return nullptr;
}

void CEconomyManager::StartFactoryTask(const float seconds)
{
	CFactoryManager* factoryMgr = circuit->GetFactoryManager();
	if (factoryMgr->GetFactoryCount() == 0) {
		CCircuitUnit* comm = circuit->GetSetupManager()->GetCommander();
		const AIFloat3 pos = (comm != nullptr) ? comm->GetPos(circuit->GetLastFrame()) : AIFloat3(-RgtVector);
		if (!factoryMgr->IsSwitchTime() && (comm != nullptr) && (comm->GetTask()->GetType() == IUnitTask::Type::BUILDER)) {
			IBuilderTask* taskB = static_cast<IBuilderTask*>(comm->GetTask());
			const float maxDist = comm->GetCircuitDef()->GetBuildDistance() + comm->GetCircuitDef()->GetSpeed() * seconds;
			if (taskB->GetPosition().SqDistance2D(pos) > SQUARE(maxDist)) {
				factoryMgr->RaiseSwitchTime();
			}
		}
		IBuilderTask* factoryTask = UpdateFactoryTasks();
		if (factoryTask == nullptr) {
			if (isEnergyRequired) {
				if (comm != nullptr) {
					UpdateEnergyTasks(pos, comm);
				}
			}
			return;
		}
//		if (comm != nullptr) {
//			circuit->GetBuilderManager()->AssignTask(comm, factoryTask);
//		}
	}

	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RemoveJob(startFactory);
	startFactory = nullptr;

	const int interval = circuit->GetAllyTeam()->GetSize() * FRAMES_PER_SEC;
	auto update = static_cast<IBuilderTask* (CEconomyManager::*)(void)>(&CEconomyManager::UpdateFactoryTasks);
	scheduler->RunJobEvery(CScheduler::GameJob(update, this),
							interval, circuit->GetSkirmishAIId() + 0 + 10 * interval);
}

void CEconomyManager::AddMorphee(CCircuitUnit* unit)
{
	if (!unit->IsUpgradable() || (unit->GetTask()->GetType() == IUnitTask::Type::NIL)) {
		return;
	}
	morphees.insert(unit);
	if (morph == nullptr) {
		morph = CScheduler::GameJob(&CEconomyManager::UpdateMorph, this);
		circuit->GetScheduler()->RunJobEvery(morph, FRAMES_PER_SEC * 10);
	}
}

void CEconomyManager::UpdateMorph()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	if (morphees.empty()) {
		circuit->GetScheduler()->RemoveJob(morph);
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
		factoryMgr->EnqueueTask(priotiry, buildDef, pos, recruit, 64.f);
	}
}

bool CEconomyManager::CheckAssistRequired(const AIFloat3& position, CCircuitUnit* unit, IBuilderTask*& outTask)
{
	outTask = nullptr;

	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	const int nanoSize = builderMgr->GetTasks(IBuilderTask::BuildType::NANO).size();
	if (nanoSize > 2) {
		return true;
	}

	CFactoryManager* factoryMgr = circuit->GetFactoryManager();
	CCircuitUnit* factory = factoryMgr->NeedUpgrade();
	if (factory == nullptr) {
		return false;
	}

	const int frame = circuit->GetLastFrame();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	AIFloat3 buildPos = factory->GetPos(frame);
	CCircuitDef* assistDef = nullptr;
	for (const auto& nano : assistDefs.infos) {  // sorted by high-tech first
		if (nano.cdef->IsAvailable(frame)
			&& terrainMgr->CanBeBuiltAtSafe(nano.cdef, buildPos))
		{
			assistDef = nano.cdef;
			break;
		}
	}
	if (assistDef == nullptr) {
		return true;
	}

	/*
	 * check metal and energy
	 */
//	const float metalIncome = std::min(GetAvgMetalIncome(), GetAvgEnergyIncome());  // FIXME: ZK
	float engyFactor;
	float metalFactor;
	CCircuitDef* reprDef = factoryMgr->GetRepresenter(factory->GetCircuitDef());
	if (reprDef == nullptr) {
		engyFactor = (GetAvgEnergyIncome() - assistDef->GetUpkeepE()) * 0.08f - assistDef->GetBuildSpeed();
		metalFactor = GetAvgMetalIncome() - assistDef->GetBuildSpeed();
	} else {
		const float buildTime = reprDef->GetBuildTime() / factory->GetCircuitDef()->GetWorkerTime();
		// resources required for nano to support factory
		const float miRequired = reprDef->GetCostM() / buildTime;
		const float eiRequired = reprDef->GetCostE() / buildTime;
		engyFactor = (GetAvgEnergyIncome() - assistDef->GetUpkeepE() - eiRequired) * 0.08f;
		metalFactor = GetAvgMetalIncome() - miRequired;
	}
	const float factoryPower = factoryMgr->GetFactoryPower() + nanoSize * factoryMgr->GetAssistSpeed();
	// FIXME: DEBUG
	circuit->LOG("%s | %f >= %f | %f >= %f", assistDef->GetDef()->GetName(), metalFactor, factoryPower, engyFactor, factoryPower);
	// FIXME: DEBUG
	if (metalFactor < factoryPower) {
		return true;
	}
	if (engyFactor < factoryPower) {
		isEnergyRequired = true;  // enough metal, request energy
		return true;
	}

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
	CTerrainManager::CorrectPosition(buildPos);
	CCircuitDef* bdef = (unit == nullptr) ? factory->GetCircuitDef() : unit->GetCircuitDef();
	buildPos = terrainMgr->GetBuildPosition(bdef, buildPos);

	if (terrainMgr->CanBeBuiltAtSafe(assistDef, buildPos)
		&& ((unit == nullptr) || terrainMgr->CanReachAtSafe(unit, buildPos, unit->GetCircuitDef()->GetBuildDistance())))
	{
		outTask = builderMgr->EnqueueTask(IBuilderTask::Priority::HIGH, assistDef, buildPos,
										  IBuilderTask::BuildType::NANO, SQUARE_SIZE * 8, true);
		return true;
	}
	return false;
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

	/*const float curMetal = */GetMetalCur();
	/*const float storMetal = */GetMetalStore();
	/*const float incMetal = */GetAvgMetalIncome();
	/*const float pullMetal = */GetMetalPull();

	/*const float curEnergy = */GetEnergyCur();
	/*const float storEnergy = */GetEnergyStore();
	/*const float incEnergy = */GetAvgEnergyIncome();
	/*const float pullEnergy = */GetEnergyPull();

	// Update isMetalEmpty, isMetalFull, isEnergyEmpty, isEnergyStalling
	static_cast<CEconomyScript*>(script)->UpdateEconomy();

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
