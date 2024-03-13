/*
 * FactoryManager.cpp
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#include "module/FactoryManager.h"
#include "module/EconomyManager.h"
#include "module/BuilderManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "scheduler/Scheduler.h"
#include "script/FactoryScript.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "task/NilTask.h"
#include "task/IdleTask.h"
#include "task/static/WaitTask.h"
#include "task/static/RepairTask.h"
#include "task/static/ReclaimTask.h"
#include "unit/FactoryData.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/Utils.h"
#include "util/Profiler.h"
#include "json/json.h"

#include "spring/SpringCallback.h"

#include "AIFloat3.h"
#include "AISCommands.h"
#include "Feature.h"
#include "Log.h"

namespace circuit {

using namespace springai;
using namespace terrain;

//#define FACTORY_CHOICE 1
#ifdef FACTORY_CHOICE
static int tierDbg;
static std::string unitTypeDbg;
#endif

CFactoryManager::CFactoryManager(CCircuitAI* circuit)
		: IUnitModule(circuit, new CFactoryScript(circuit->GetScriptManager(), this))
		, metalRequire(0.f)
		, energyRequire(0.f)
		, isAssistRequired(false)
		, isSwitchTime(false)
		, lastSwitchFrame(-1)
		, noT1FacCount(0)
		, bpRatio(1.f)
		, reWeight(.5f)
{
	circuit->GetScheduler()->RunOnInit(CScheduler::GameJob(&CFactoryManager::Init, this));

	/*
	 * factory handlers
	 */
	auto factoryCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nilTask->AssignTo(unit);
			this->circuit->AddActionUnit(unit);
		}

		CTerrainManager* terrainMgr = this->circuit->GetTerrainManager();
		terrainMgr->AddZoneOwn(unit->GetPos(this->circuit->GetLastFrame()));

		// Mark path from factory to lanePos as blocked
		if (unit->GetCircuitDef()->GetMobileId() >= 0) {  // no air factory
			CSetupManager* setupMgr = this->circuit->GetSetupManager();
			CCircuitDef* reprDef = GetRepresenter(unit->GetCircuitDef());
			if (reprDef == nullptr) {
				reprDef = setupMgr->GetCommChoice();
			}
			terrainMgr->AddBusPath(unit, setupMgr->GetLanePos(), reprDef);
		}
	};
	auto factoryFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			this->circuit->AddActionUnit(unit);
		}
		nilTask->RemoveAssignee(unit);
		idleTask->AssignTo(unit);

		TRY_UNIT(this->circuit, unit,
			unit->GetUnit()->SetRepeat(false);
			unit->GetUnit()->SetIdleMode(0);
		)

		this->circuit->GetEconomyManager()->AddFactoryInfo(unit);
		UnitAdded(unit, UseAs::FACTORY);

		lastSwitchFrame = this->circuit->GetLastFrame();
		EnableFactory(unit);
	};
	auto factoryIdleHandler = [](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto factoryDestroyedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		// NOTE: Do not del if factory rotation wanted
		DelFactory(unit->GetCircuitDef());
		DisableFactory(unit);
		this->circuit->GetEconomyManager()->DelFactoryInfo(unit);

		CTerrainManager* terrainMgr = this->circuit->GetTerrainManager();
		terrainMgr->DelZoneOwn(unit->GetPos(this->circuit->GetLastFrame()));
		// Remove blocked path from factory to lanePos
		terrainMgr->DelBusPath(unit);

		UnitRemoved(unit, UseAs::FACTORY);
	};

	/*
	 * armnanotc handlers
	 */
	auto assistCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nilTask->AssignTo(unit);
			this->circuit->AddActionUnit(unit);
		}
	};
	auto assistFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			this->circuit->AddActionUnit(unit);
		}
		nilTask->RemoveAssignee(unit);
		idleTask->AssignTo(unit);

		int frame = this->circuit->GetLastFrame();
		const AIFloat3& assPos = unit->GetPos(frame);
		TRY_UNIT(this->circuit, unit,
			unit->CmdPriority(0);
			// FIXME: BA
			if (unit->GetCircuitDef()->IsRoleSupport()) {
				unit->CmdBARPriority(0);
			}
			// FIXME: BA
		)

		// check factory nano belongs to
		const float radius = unit->GetCircuitDef()->GetBuildDistance() * 0.9f;
		const float sqRadius = SQUARE(radius);
		SAssistToFactory& af = assists[unit];
		for (SFactory& fac : factories) {
			if (assPos.SqDistance2D(fac.unit->GetPos(frame)) >= sqRadius) {
				continue;
			}
			auto it = fac.nanos.find(unit->GetCircuitDef());
			if (it == fac.nanos.end()) {
				fac.nanos[unit->GetCircuitDef()].incomeMod = unit->GetCircuitDef()->GetWorkerTime() / fac.unit->GetCircuitDef()->GetWorkerTime();
			}
			SAssistant& assist = fac.nanos[unit->GetCircuitDef()];
			const float metalUse = fac.miRequire * assist.incomeMod;
			const float energyUse = fac.eiRequire * assist.incomeMod + unit->GetCircuitDef()->GetUpkeepE();
			af.metalRequire = std::max(af.metalRequire, metalUse);
			af.energyRequire = std::max(af.energyRequire, energyUse);
			fac.miRequireTotal += metalUse;
			fac.eiRequireTotal += energyUse;
			assist.units.insert(unit);
			++fac.nanoSize;
			af.factories.insert(fac.unit);
		}
		if (!af.factories.empty()) {
			metalRequire += af.metalRequire;
			energyRequire += af.energyRequire;

			bool isInHaven = false;
			for (const AIFloat3& hav : havens) {
				if (assPos.SqDistance2D(hav) < sqRadius) {
					isInHaven = true;
					break;
				}
			}
			if (!isInHaven) {
				havens.push_back(assPos);
			}
		}

		UnitAdded(unit, UseAs::ASSIST);
	};
	auto assistIdleHandler = [](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto assistDestroyedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		if (task->GetType() == IUnitTask::Type::NIL) {
			return;
		}
		const AIFloat3& assPos = unit->GetPos(this->circuit->GetLastFrame());
		const float radius = unit->GetCircuitDef()->GetBuildDistance();
		const float sqRadius = SQUARE(radius);
		for (SFactory& fac : factories) {
			auto fit = fac.nanos.find(unit->GetCircuitDef());
			if (fit == fac.nanos.end()) {
				continue;
			}
			SAssistant& assist = fit->second;
			if (assist.units.erase(unit) == 0) {
				continue;
			}
			const float metalUse = fac.miRequire * assist.incomeMod;
			const float energyUse = fac.eiRequire * assist.incomeMod + unit->GetCircuitDef()->GetUpkeepE();
			fac.miRequireTotal -= metalUse;
			fac.eiRequireTotal -= energyUse;
			if (--fac.nanoSize > 0) {
				continue;
			}
			auto it = havens.begin();
			while (it != havens.end()) {
				if (it->SqDistance2D(assPos) < sqRadius) {
					*it = havens.back();
					havens.pop_back();
				} else {
					++it;
				}
			}
		}
		SAssistToFactory& af = assists[unit];
		if (!af.factories.empty()) {
			metalRequire -= af.metalRequire;
			energyRequire -= af.energyRequire;
		}
		assists.erase(unit);

		UnitRemoved(unit, UseAs::ASSIST);
	};

	for (CCircuitDef& cdef : circuit->GetCircuitDefs()) {
		// Auto-assign roles
		auto setRoles = [circuit, &cdef](CCircuitDef::RoleT type) {
			if (circuit->GetBindedRole(cdef.GetMainRole()) != type) {
				cdef.SetMainRole(type);
				cdef.AddEnemyRole(type);
				cdef.AddRole(type);
			}
		};
		if (cdef.IsAbleToFly()) {
			setRoles(ROLE_TYPE(AIR));
		} else if (!cdef.IsMobile() && cdef.IsAttacker() && cdef.HasSurfToLand()) {
			setRoles(ROLE_TYPE(STATIC));
		} else if (cdef.GetDef()->IsBuilder() && cdef.IsBuilder() && !cdef.IsRoleComm()) {
			setRoles(ROLE_TYPE(BUILDER));
		}
		if (cdef.IsRoleComm()) {
			// NOTE: Omit AddRole to exclude commanders from response
//			cdef.SetMainRole(ROLE_TYPE(BUILDER));  // breaks retreat
			cdef.AddEnemyRole(ROLE_TYPE(COMM));
			cdef.AddEnemyRole(ROLE_TYPE(BUILDER));
		}
	}

	ReadConfig();

	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	for (CCircuitDef& cdef : circuit->GetCircuitDefs()) {
		if (cdef.IsMobile() || !cdef.GetDef()->IsBuilder()) {
			continue;
		}

		CCircuitDef::Id unitDefId = cdef.GetId();
		// FIXME: Caretaker can be factory. Make attributes?
		if (cdef.IsBuilder() && (factoryDefs.find(unitDefId) != factoryDefs.end())) {
			createdHandler[unitDefId] = factoryCreatedHandler;
			finishedHandler[unitDefId] = factoryFinishedHandler;
			idleHandler[unitDefId] = factoryIdleHandler;
			destroyedHandler[unitDefId] = factoryDestroyedHandler;
			economyMgr->AddFactoryDef(&cdef);
		} else if (cdef.IsAbleToAssist()
			&& (std::max(cdef.GetDef()->GetXSize(), cdef.GetDef()->GetZSize()) * SQUARE_SIZE < cdef.GetBuildDistance()))
		{
			createdHandler[unitDefId] = assistCreatedHandler;
			finishedHandler[unitDefId] = assistFinishedHandler;
			idleHandler[unitDefId] = assistIdleHandler;
			destroyedHandler[unitDefId] = assistDestroyedHandler;
			economyMgr->AddAssistDef(&cdef);
		}
	}

	factoryData = circuit->GetAllyTeam()->GetFactoryData().get();
}

CFactoryManager::~CFactoryManager()
{
}

void CFactoryManager::ReadConfig()
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();

	CMaskHandler& sideMasker = circuit->GetGameAttribute()->GetSideMasker();
	sideInfos.resize(sideMasker.GetMasks().size());

	const Json::Value& econom = root["economy"];
	const Json::Value& nanotc = econom["assist"];
	for (const auto& kv : sideMasker.GetMasks()) {
		SSideInfo& sideInfo = sideInfos[kv.second.type];

		const std::string& nanoName = nanotc.get(kv.first, "").asString();
		CCircuitDef* assistDef = circuit->GetCircuitDef(nanoName.c_str());
		if (assistDef == nullptr) {
			assistDef = circuit->GetEconomyManager()->GetSideInfo().defaultDef;
			circuit->LOG("CONFIG %s: has unknown assistDef '%s'", cfgName.c_str(), nanoName.c_str());
		}
		sideInfo.assistDef = assistDef;
	}

	const Json::Value& product = econom["production"];
	newFacModM = product.get((unsigned)0, 0.8f).asFloat();
	newFacModE = product.get((unsigned)1, 0.8f).asFloat();
	facModM = product.get((unsigned)2, 0.8f).asFloat();
	facModE = product.get((unsigned)3, 0.8f).asFloat();

	numBatch = root["quota"].get("num_batch", 5).asInt();

	/*
	 * Roles, attributes and retreat
	 */
	std::map<CCircuitDef::RoleT, std::set<CCircuitDef::Id>> roleDefs;
	CCircuitDef::RoleName& roleNames = CCircuitDef::GetRoleNames();
	CCircuitDef::AttrName& attrNames = CCircuitDef::GetAttrNames();
	CCircuitDef::FireName& fireNames = CCircuitDef::GetFireNames();
	std::set<CCircuitDef::RoleT> modRoles;
	const Json::Value& behaviours = root["behaviour"];
	for (const std::string& defName : behaviours.getMemberNames()) {
		CCircuitDef* cdef = circuit->GetCircuitDef(defName.c_str());
		if (cdef == nullptr) {
			circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), defName.c_str());
			continue;
		}

		// Read roles from config
		const Json::Value& behaviour = behaviours[defName];
		const Json::Value& role = behaviour["role"];
		if (role.empty()) {
			circuit->LOG("CONFIG %s: '%s' has no role", cfgName.c_str(), defName.c_str());
			continue;
		}

		const std::string& mainName = role[0].asString();
		auto it = roleNames.find(mainName);
		if (it == roleNames.end()) {
			circuit->LOG("CONFIG %s: %s has unknown main role '%s'", cfgName.c_str(), defName.c_str(), mainName.c_str());
			continue;
		}
		cdef->SetMainRole(it->second.type);
		cdef->AddRole(it->second.type, circuit->GetBindedRole(it->second.type));
		roleDefs[it->second.type].insert(cdef->GetId());

//		if (role.size() < 2) {
			cdef->AddEnemyRoles(it->second.mask);
//		} else {
			for (unsigned i = 1; i < role.size(); ++i) {
				const std::string& enemyName = role[i].asString();
				it = roleNames.find(enemyName);
				if (it == roleNames.end()) {
					circuit->LOG("CONFIG %s: %s has unknown enemy role '%s'", cfgName.c_str(), defName.c_str(), enemyName.c_str());
					continue;
				}
				cdef->AddEnemyRoles(it->second.mask);
//				cdef->AddRole(it->second.type, circuit->GetBindedRole(it->second.type));
			}
//		}

		// Read optional roles and attributes
		const Json::Value& attributes = behaviour["attribute"];
		for (const Json::Value& attr : attributes) {
			const std::string& attrName = attr.asString();
			it = roleNames.find(attrName);
			if (it == roleNames.end()) {
				auto it = attrNames.find(attrName);
				if (it == attrNames.end()) {
					circuit->LOG("CONFIG %s: %s has unknown attribute '%s'", cfgName.c_str(), defName.c_str(), attrName.c_str());
					continue;
				} else {
					cdef->AddAttribute(it->second.type);
				}
			} else {
				cdef->AddRole(it->second.type, circuit->GetBindedRole(it->second.type));
				roleDefs[it->second.type].insert(cdef->GetId());
			}
		}
		if (cdef->IsAttrNoDGun()) {
			cdef->RemDGun();
		}

		const Json::Value& fire = behaviour["fire_state"];
		if (!fire.isNull()) {
			const std::string& fireName = fire.asString();
			auto itf = fireNames.find(fireName);
			if (itf == fireNames.end()) {
				circuit->LOG("CONFIG %s: %s has unknown fire state '%s'", cfgName.c_str(), defName.c_str(), fireName.c_str());
			} else {
				cdef->SetFireState(itf->second);
			}
		}

		const Json::Value& slowOnOff = behaviour["slow_target"];
		if (!slowOnOff.isNull()) {
			cdef->AddAttribute(ATTR_TYPE(ONOFF));
			cdef->SetOnSlow(slowOnOff.asBool());
		}
		cdef->SetOn(behaviour.get("on", true).asBool());

		const Json::Value& reload = behaviour["reload"];
		if (!reload.isNull()) {
			cdef->SetReloadTime(reload.asFloat() * FRAMES_PER_SEC);
		}

		const Json::Value& limit = behaviour["limit"];
		if (!limit.isNull()) {
			cdef->SetMaxThisUnit(std::min(limit.asInt(), cdef->GetMaxThisUnit()));
		}

		const Json::Value& since = behaviour["since"];
		if (!since.isNull()) {
			cdef->SetSinceFrame(since.asInt() * FRAMES_PER_SEC);
		}

		const Json::Value& coold = behaviour["cooldown"];
		if (!coold.isNull()) {
			cdef->SetCooldown(coold.asInt() * FRAMES_PER_SEC);
		}

		cdef->SetGoalBuildMod(behaviour.get("build_mod", cdef->GetGoalBuildMod()).asFloat());

		const Json::Value& retreat = behaviour.get("retreat", cdef->GetRetreat());
		if (retreat.isNumeric()) {
			cdef->SetRetreat(retreat.asFloat());
		} else {
			const float min = retreat.get((unsigned)0, cdef->GetRetreat()).asFloat();
			const float max = retreat.get((unsigned)1, cdef->GetRetreat()).asFloat();
			cdef->SetRetreat((float)rand() / RAND_MAX * (max - min) + min);
		}

		const Json::Value& pwrMod = behaviour["power"];
		if (!pwrMod.isNull()) {
			cdef->ModPower(pwrMod.asFloat());
		}
		const Json::Value& thrMod = behaviour["threat"];
		if (!thrMod.isNull()) {
			if (thrMod.isNumeric()) {
				const float mod = thrMod.asFloat();
				cdef->ModDefThreat(mod);
				cdef->ModAirThreat(mod);
				cdef->ModSurfThreat(mod);
				cdef->ModWaterThreat(mod);
			} else if (thrMod.isObject()) {
				cdef->ModAirThreat(thrMod.get("air", 1.f).asFloat());
				cdef->ModSurfThreat(thrMod.get("surf", 1.f).asFloat());
				cdef->ModWaterThreat(thrMod.get("water", 1.f).asFloat());
				const float defMod = thrMod.get("default", 1.f).asFloat();
				cdef->ModDefThreat(defMod);
				const Json::Value& thrRole = thrMod["vs"];
				if (!thrRole.isNull()) {
					for (auto& kv : roleNames) {
						const float rolMod = thrRole.get(kv.first, defMod).asFloat();
						if (rolMod != defMod) {
							modRoles.insert(kv.second.type);
						}
						cdef->ModThreatMod(kv.second.type, rolMod);
					}
				}
			}
		}

		cdef->SetIgnore(behaviour.get("ignore", cdef->IsIgnore()).asBool());

		const Json::Value& mpOffset = behaviour["midposoffset"];
		if (!mpOffset.isNull()) {
			float x = mpOffset.get((unsigned)0, 0.f).asFloat();
			float y = mpOffset.get((unsigned)1, 0.f).asFloat();
			float z = mpOffset.get((unsigned)2, 0.f).asFloat();
			cdef->SetMidPosOffset(x, y, z);
		}

		const Json::Value& buildSpeed = behaviour["build_speed"];
		if (!buildSpeed.isNull()) {
			cdef->SetBuildSpeed(buildSpeed.asFloat());
		}
	}
	circuit->GetAllyTeam()->NonDefaultThreats(std::move(modRoles), circuit);

	/*
	 * Factories
	 */
	const bool warnProb = root.get("warn_probability", true).asBool();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	const Json::Value& factories = root["factory"];
	for (const std::string& fac : factories.getMemberNames()) {
		CCircuitDef* cdef = circuit->GetCircuitDef(fac.c_str());
		if (cdef == nullptr) {
			circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), fac.c_str());
			continue;
		}

		const Json::Value& factory = factories[fac];
		SFactoryDef facDef;

		// FIXME: used to create tasks on Event (like DefendTask)
//		const std::unordered_set<CCircuitDef::Id>& options = cdef->GetBuildOptions();
//		const CCircuitDef::RoleT roleSize = roleNames.size();
//		facDef.roleDefs.resize(roleSize, nullptr);
//		for (CCircuitDef::RoleT type = 0; type < roleSize; ++type) {
//			float minCost = std::numeric_limits<float>::max();
//			CCircuitDef* rdef = nullptr;
//			const std::set<CCircuitDef::Id>& defIds = roleDefs[type];
//			for (const CCircuitDef::Id bid : defIds) {
//				if (options.find(bid) == options.end()) {
//					continue;
//				}
//				CCircuitDef* tdef = circuit->GetCircuitDef(bid);
//				if (minCost > tdef->GetCostM()) {
//					minCost = tdef->GetCostM();
//					rdef = tdef;
//				}
//			}
//			facDef.roleDefs[type] = rdef;
//		}

		facDef.isRequireEnergy = factory.get("require_energy", false).asBool();

		const Json::Value& items = factory["unit"];
		const Json::Value& tiers = factory["income_tier"];
		facDef.buildDefs.reserve(items.size());
		const unsigned tierSize = tiers.size();
		facDef.incomes.reserve(tierSize + 1);

		CCircuitDef* landDef = nullptr;
		CCircuitDef* waterDef = nullptr;
		float landSize = std::numeric_limits<float>::max();
		float waterSize = std::numeric_limits<float>::max();
		std::vector<unsigned> trueIndex;

		for (unsigned i = 0; i < items.size(); ++i) {
			CCircuitDef* udef = circuit->GetCircuitDef(items[i].asCString());
			if (udef == nullptr) {
				circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), items[i].asCString());
				continue;
			}
			if (!cdef->CanBuild(udef)) {  // buildlist from modoptions
				circuit->LOG("CONFIG %s: factory '%s' can't build '%s'", cfgName.c_str(), fac.c_str(), items[i].asCString());
				continue;
			}
			trueIndex.push_back(i);
			facDef.buildDefs.push_back(udef);

			// identify surface representatives
			if (udef->GetMobileId() < 0) {
				if (landDef == nullptr) {
					landDef = udef;
				}
				if (waterDef == nullptr) {
					waterDef = udef;
				}
				continue;
			}
			SArea* area = terrainMgr->GetMobileTypeById(udef->GetMobileId())->areaLargest;
			if (area == nullptr) {
				continue;
			}
			if ((area->mobileType->maxElevation > 0.f) && (landSize > area->percentOfMap)) {
				landSize = area->percentOfMap;
				landDef = udef;
			}
			if (((area->mobileType->minElevation < 0.f) || udef->IsFloater()) && (waterSize > area->percentOfMap)) {
				waterSize = area->percentOfMap;
				waterDef = udef;
			}
		}
		if (facDef.buildDefs.empty()) {
			continue;  // ignore empty factory
		}
		facDef.landDef = landDef;
		facDef.waterDef = waterDef;

		auto fillProbs = [this, &cfgName, &facDef, &fac, &factory, &trueIndex, warnProb](unsigned i, const char* type, SFactoryDef::Tiers& tiers) {
			const Json::Value& tierType = factory[type];
			if (tierType.isNull()) {
				return false;
			}
			const Json::Value& tier = tierType[utils::int_to_string(i, "tier%i")];
			if (tier.isNull()) {
				return false;
			}
			std::vector<float>& probs = tiers[i];
			probs.reserve(facDef.buildDefs.size());
			float sum = .0f;
			for (unsigned j : trueIndex) {
				const float p = tier[j].asFloat();
				sum += p;
				probs.push_back(p);
			}
			if (warnProb && (std::fabs(sum - 1.0f) > 0.0001f)) {
				circuit->LOG("CONFIG %s: %s's %s_tier%i total probability = %f", cfgName.c_str(), fac.c_str(), type, i, sum);
			}
			return true;
		};
		unsigned i = 0;
		for (; i < tierSize; ++i) {
			facDef.incomes.push_back(tiers[i].asFloat());
			fillProbs(i, "air", facDef.airTiers);
			fillProbs(i, "land", facDef.landTiers);
			fillProbs(i, "water", facDef.waterTiers);
		}
		fillProbs(i, "air", facDef.airTiers);
		fillProbs(i, "land", facDef.landTiers);
		fillProbs(i, "water", facDef.waterTiers);

//		if (facDef.incomes.empty()) {
			facDef.incomes.push_back(std::numeric_limits<float>::max());
//		}
		if (facDef.landTiers.empty()) {
			if (!facDef.airTiers.empty()) {
				facDef.landTiers = facDef.airTiers;
			} else if (!facDef.waterTiers.empty()) {
				facDef.landTiers = facDef.waterTiers;
			} else {
				facDef.landTiers[0];  // create empty tier
			}
		}
		if (facDef.waterTiers.empty()) {
			facDef.waterTiers = facDef.landTiers;
		}
		if (facDef.airTiers.empty()) {
			facDef.airTiers = terrainMgr->IsWaterMap() ? facDef.waterTiers : facDef.landTiers;
		}

		facDef.nanoCount = factory.get("caretaker", 1).asUInt();

		factoryDefs[cdef->GetId()] = facDef;
	}

	bpRatio = root["economy"].get("buildpower", 1.f).asFloat();
	reWeight = root["response"].get("_weight_", .5f).asFloat();
}

void CFactoryManager::Init()
{
	CSetupManager::StartFunc subinit = [this](const AIFloat3& pos) {
		CScheduler* scheduler = circuit->GetScheduler().get();
		const int interval = 4;
		const int offset = circuit->GetSkirmishAIId() % interval;
		scheduler->RunJobEvery(CScheduler::GameJob(&CFactoryManager::UpdateIdle, this), interval, offset + 0);
		scheduler->RunJobEvery(CScheduler::GameJob(&CFactoryManager::Update, this), interval, offset + 2);

		scheduler->RunJobEvery(CScheduler::GameJob(&CFactoryManager::Watchdog, this),
								FRAMES_PER_SEC * 60,
								circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 11);
	};

	circuit->GetSetupManager()->ExecOnFindStart(subinit);
}

int CFactoryManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	CCircuitDef* cdef = unit->GetCircuitDef();
	TRY_UNIT(circuit, unit,
		unit->GetUnit()->SetFireState(cdef->GetFireState());
	)

	auto search = createdHandler.find(cdef->GetId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	if (unit->GetTask() == nullptr) {  // all units without handlers
		unit->SetManager(this);
		nilTask->AssignTo(unit);
		circuit->AddActionUnit(unit);
	}

	if (builder == nullptr) {
		return 0; //signaling: OK
	}

	IUnitTask* task = builder->GetTask();
	if ((task == nullptr) || (task->GetType() != IUnitTask::Type::FACTORY)) {
		return 0; //signaling: OK
	}

	if (unit->GetUnit()->IsBeingBuilt()) {
		CRecruitTask* taskR = static_cast<CRecruitTask*>(task);
		if (taskR->GetTarget() == nullptr) {
			taskR->SetTarget(unit);
			unfinishedUnits[unit] = taskR;
		}
	}

	return 0; //signaling: OK
}

int CFactoryManager::UnitFinished(CCircuitUnit* unit)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		DoneTask(iter->second);
	}
	auto itre = repairUnits.find(unit->GetId());
	if (itre != repairUnits.end()) {
		DoneTask(itre->second);
	}

	auto search = finishedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CFactoryManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetCircuitDef()->GetId());
	if (search != idleHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CFactoryManager::UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		AbortTask(iter->second);
	}
	auto itre = repairUnits.find(unit->GetId());
	if (itre != repairUnits.end()) {
		AbortTask(itre->second);
	}

	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

CRecruitTask* CFactoryManager::Enqueue(const TaskS::SRecruitTask& ti)
{
	CRecruitTask* task = new CRecruitTask(this, ti.priority, ti.buildDef, ti.position, ti.type, ti.radius);
	factoryTasks.push_back(task);
	updateTasks.push_back(task);
	TaskAdded(task);
	return task;
}

IUnitTask* CFactoryManager::Enqueue(const TaskS::SServSTask& ti)
{
	IUnitTask* task;

	switch (ti.type) {
		case IBuilderTask::BuildType::REPAIR: {
			auto it = repairUnits.find(ti.target->GetId());
			if (it != repairUnits.end()) {
				return it->second;
			}
			task = new CSRepairTask(this, ti.priority, ti.target);
		} break;
		case IBuilderTask::BuildType::RECLAIM: {
			task = new CSReclaimTask(this, ti.priority, ti.position, {0.f, 0.f}, ti.timeout, ti.radius);
		} break;
		default:
		case IBuilderTask::BuildType::WAIT: {
			task = new CSWaitTask(this, ti.stop, ti.timeout);
		} break;
	}

	updateTasks.push_back(task);
	TaskAdded(task);
	return task;
}

void CFactoryManager::DequeueTask(IUnitTask* task, bool done)
{
	switch (task->GetType()) {
		case IUnitTask::Type::FACTORY:{
			switch (static_cast<IBuilderTask*>(task)->GetBuildType()) {
				case IBuilderTask::BuildType::RECRUIT: {
					auto it = std::find(factoryTasks.begin(), factoryTasks.end(), task);
					if (it != factoryTasks.end()) {
						factoryTasks.erase(it);
					}
					unfinishedUnits.erase(static_cast<CRecruitTask*>(task)->GetTarget());
				} break;
				case IBuilderTask::BuildType::REPAIR: {
					repairUnits.erase(static_cast<CSRepairTask*>(task)->GetTargetId());
				} break;
				default: break;  // RECLAIM
			}
		} break;
		default: break;
	}  // WAIT
	IUnitModule::DequeueTask(task, done);
}

void CFactoryManager::ApplySwitchFrame()
{
	lastSwitchFrame = circuit->GetLastFrame();
	isSwitchTime = false;
}

bool CFactoryManager::IsSwitchTime()
{
	if (!isSwitchTime) {
		isSwitchTime = static_cast<CFactoryScript*>(script)->IsSwitchTime(lastSwitchFrame);
	}
	return isSwitchTime;
}

bool CFactoryManager::IsSwitchAllowed(CCircuitDef* facDef) const
{
	return static_cast<CFactoryScript*>(script)->IsSwitchAllowed(facDef);
}

CCircuitUnit* CFactoryManager::NeedUpgrade(unsigned int nanoQueued)
{
	const int frame = circuit->GetLastFrame();
	unsigned facSize = factories.size();
	for (auto itF = factories.rbegin(); itF != factories.rend(); ++itF) {
		SFactory& fac = *itF;

		auto itD = factoryDefs.find(fac.unit->GetCircuitDef()->GetId());
		if (itD != factoryDefs.end()) {
			const SFactoryDef& facDef = itD->second;

			for (CCircuitDef* cdef : facDef.buildDefs) {
				if (cdef->IsAvailable(frame)) {
					if (fac.nanoSize + nanoQueued < facSize * fac.weight) {
						return fac.unit;
					} else {
						break;
					}
				}
			}
		}
	}
	return nullptr;
}

CCircuitUnit* CFactoryManager::GetClosestFactory(const AIFloat3& position)
{
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
//	CTerrainManager::CorrectPosition(position);
	const int iS = terrainMgr->GetSectorIndex(position);
	CCircuitUnit* factory = nullptr;
	float minSqDist = std::numeric_limits<float>::max();
	const int frame = circuit->GetLastFrame();
	for (SFactory& fac : factories) {
		if (factoryData->IsT1Factory(fac.unit->GetCircuitDef())) {
			continue;
		}
		SArea* area = fac.unit->GetArea();
		if ((area != nullptr) && (area->sector.find(iS) == area->sector.end())) {
			continue;
		}
		const AIFloat3& facPos = fac.unit->GetPos(frame);
		float sqDist = position.SqDistance2D(facPos);
		if (minSqDist > sqDist) {
			minSqDist = sqDist;
			factory = fac.unit;
		}
	}
	// FIXME: DEBUG lazy t1 factory check
	if (factory == nullptr) {
		for (SFactory& fac : factories) {
			if (!factoryData->IsT1Factory(fac.unit->GetCircuitDef())) {
				continue;
			}
			SArea* area = fac.unit->GetArea();
			if ((area != nullptr) && (area->sector.find(iS) == area->sector.end())) {
				continue;
			}
			const AIFloat3& facPos = fac.unit->GetPos(frame);
			float sqDist = position.SqDistance2D(facPos);
			if (minSqDist > sqDist) {
				minSqDist = sqDist;
				factory = fac.unit;
			}
		}
	}
	// FIXME: DEBUG
	return factory;
}

//CCircuitDef* CFactoryManager::GetClosestDef(AIFloat3& position, CCircuitDef::RoleT role)
//{
//	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
//	CTerrainManager::CorrectPosition(position);
//	int iS = terrainMgr->GetSectorIndex(position);
//	CCircuitDef* roleDef = nullptr;
//	float minSqDist = std::numeric_limits<float>::max();
//	const int frame = circuit->GetLastFrame();
//	for (SFactory& fac : factories) {
//		STerrainMapArea* area = fac.unit->GetArea();
//		if ((area != nullptr) && (area->sector.find(iS) == area->sector.end())) {
//			continue;
//		}
//		const AIFloat3& facPos = fac.unit->GetPos(frame);
//		float sqDist = position.SqDistance2D(facPos);
//		if (minSqDist < sqDist) {
//			continue;
//		}
//		const SFactoryDef& facDef = factoryDefs.find(fac.unit->GetCircuitDef()->GetId())->second;
//		CCircuitDef* cdef = GetFacRoleDef(role, facDef);
//		if (cdef != nullptr) {
//			roleDef = cdef;
//			minSqDist = sqDist;
//			position = facPos;
//		}
//	}
//	return roleDef;
//}

AIFloat3 CFactoryManager::GetClosestHaven(CCircuitUnit* unit) const
{
	if (havens.empty()) {
		return -RgtVector;
	}
	float metric = std::numeric_limits<float>::max();
	const AIFloat3& position = unit->GetPos(circuit->GetLastFrame());
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	auto it = havens.begin(), havIt = havens.end();
	for (; it != havens.end(); ++it) {
		if (!terrainMgr->CanMoveToPos(unit->GetArea(), *it)) {
			continue;
		}
		float qdist = it->SqDistance2D(position);
		if (qdist < metric) {
			havIt = it;
			metric = qdist;
		}
	}
	return (havIt != havens.end()) ? *havIt : AIFloat3(-RgtVector);
}

AIFloat3 CFactoryManager::GetClosestHaven(const AIFloat3& position) const
{
	if (havens.empty()) {
		return -RgtVector;
	}
	float metric = std::numeric_limits<float>::max();
	auto it = havens.begin(), havIt = havens.end();
	for (; it != havens.end(); ++it) {
		float qdist = it->SqDistance2D(position);
		if (qdist < metric) {
			havIt = it;
			metric = qdist;
		}
	}
	return (havIt != havens.end()) ? *havIt : AIFloat3(-RgtVector);
}

const CFactoryManager::SSideInfo& CFactoryManager::GetSideInfo() const
{
	return sideInfos[circuit->GetSideId()];
}

CRecruitTask* CFactoryManager::UpdateBuildPower(CCircuitUnit* unit, bool isActive)
{
	if (!CanEnqueueTask()) {
		return nullptr;
	}

	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	const float metalIncome = std::min(economyMgr->GetAvgMetalIncome(), economyMgr->GetAvgEnergyIncome());
	const int r = rand();
	if ((circuit->GetBuilderManager()->GetBuildPower() >= metalIncome * bpRatio)
		|| (isActive && (r >= RAND_MAX / 2)))
	{
		return nullptr;
	}

	auto it = factoryDefs.find(unit->GetCircuitDef()->GetId());
	if (it == factoryDefs.end()) {
		return nullptr;
	}
	CCircuitDef* buildDef = GetFacRoleDef(ROLE_TYPE(BUILDER), it->second);

	if ((buildDef != nullptr) && buildDef->IsAvailable(circuit->GetLastFrame())) {
		const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
		CTerrainManager* terrainMgr = circuit->GetTerrainManager();
		if (!terrainMgr->CanBeBuiltAt(buildDef, pos, unit->GetCircuitDef()->GetBuildDistance())) {
			return nullptr;
		}
#ifdef FACTORY_CHOICE
		circuit->LOG("choice = %s | %f < %f or (%i and %i < %i)", buildDef->GetDef()->GetName(),
				circuit->GetBuilderManager()->GetBuildPower(), metalIncome * bpRatio, isActive, r, RAND_MAX / 2);
#endif
		return Enqueue(TaskS::Recruit(CRecruitTask::RecruitType::BUILDPOWER, CRecruitTask::Priority::NORMAL, buildDef, pos, 64.f));
	}
	return nullptr;
}

CRecruitTask* CFactoryManager::UpdateFirePower(CCircuitUnit* builder, bool isActive)
{
	if (!CanEnqueueTask()) {
		return nullptr;
	}

	CFactoryManager* factoryMgr = circuit->IsSlave() ? circuit->GetAllyTeam()->GetLeader()->GetFactoryManager() : this;
	SRecruitDef result = factoryMgr->RequiredFireDef(builder, isActive);
	if (result.id < 0) {
		return nullptr;
	}

	CCircuitDef* buildDef = circuit->GetCircuitDef(result.id);
	const AIFloat3& pos = builder->GetPos(circuit->GetLastFrame());
	UnitDef* def = builder->GetCircuitDef()->GetDef();
	float radius = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE / 2;
	// FIXME CCircuitDef::RoleType <-> CRecruitTask::RecruitType relations
	return Enqueue(TaskS::Recruit(CRecruitTask::RecruitType::FIREPOWER, result.priority, buildDef, pos, radius));
}

bool CFactoryManager::IsHighPriority(CAllyUnit* unit) const
{
	auto it = unfinishedUnits.find(unit);
	if (it == unfinishedUnits.end()) {
		return false;
	}
	return IBuilderTask::Priority::HIGH == it->second->GetPriority();
}

CCircuitDef* CFactoryManager::GetFactoryToBuild(AIFloat3 position, bool isStart, bool isReset)
{
	CCircuitDef* facDef = factoryData->GetFactoryToBuild(circuit, position, isStart, isReset);
	if ((facDef == nullptr) && utils::is_valid(position)) {
		facDef = factoryData->GetFactoryToBuild(circuit, -RgtVector, isStart, isReset);
	}
	return facDef;
}

void CFactoryManager::AddFactory(const CCircuitDef* cdef)
{
	factoryData->AddFactory(cdef);
}

void CFactoryManager::DelFactory(const CCircuitDef* cdef)
{
	factoryData->DelFactory(cdef);
}

CCircuitDef* CFactoryManager::GetRoleDef(const CCircuitDef* facDef, CCircuitDef::RoleT role) const
{
	auto it = factoryDefs.find(facDef->GetId());
	return (it != factoryDefs.end()) ? GetFacRoleDef(role, it->second) : nullptr;
}

CCircuitDef* CFactoryManager::GetLandDef(const CCircuitDef* facDef) const
{
	auto it = factoryDefs.find(facDef->GetId());
	return (it != factoryDefs.end()) ? it->second.landDef : nullptr;
}

CCircuitDef* CFactoryManager::GetWaterDef(const CCircuitDef* facDef) const
{
	auto it = factoryDefs.find(facDef->GetId());
	return (it != factoryDefs.end()) ? it->second.waterDef : nullptr;
}

CCircuitDef* CFactoryManager::GetRepresenter(const CCircuitDef* facDef) const
{
	CCircuitDef* landDef = GetLandDef(facDef);
	if (landDef != nullptr) {
		if (landDef->GetMobileId() < 0) {
			return landDef;
		} else {
			CTerrainManager* terrainMgr = circuit->GetTerrainManager();
			SArea* area = terrainMgr->GetMobileTypeById(landDef->GetMobileId())->areaLargest;
			// FIXME: area->percentOfMap < 40.0 doesn't seem right as water identifier
			return ((area == nullptr) || (area->percentOfMap < terrainMgr->GetMinLandPercent())) ? GetWaterDef(facDef) : landDef;
		}
	} else {
		return GetWaterDef(facDef);
	}
}

void CFactoryManager::EnableFactory(CCircuitUnit* unit)
{
	// check nanos around
	std::map<CCircuitDef*, SAssistant> nanos;
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	COOAICallback* clb = circuit->GetCallback();
	auto& units = clb->GetFriendlyUnitsIn(pos, GetAssistRange() * 0.9f);
	int teamId = circuit->GetTeamId();
	for (Unit* nano : units) {
		int unitId = nano->GetUnitId();
		CCircuitDef* nDef = circuit->GetCircuitDef(clb->Unit_GetDefId(unitId));
		if (nDef->IsAssist() && (nano->GetTeam() == teamId) && !nano->IsBeingBuilt()) {
			CCircuitUnit* ass = circuit->GetTeamUnit(unitId);
			// NOTE: OOAICallback::GetFriendlyUnits may return yet unregistered units created in GamePreload
			if (ass != nullptr) {
				SAssistant& assist = nanos[nDef];
				assist.units.insert(ass);
			}
		}
		delete nano;
	}
//	utils::free_clear(units);

	float facWorkerTime = unit->GetCircuitDef()->GetWorkerTime();
	float miRequire;
	float eiRequire;
	CCircuitDef* reprDef = GetRepresenter(unit->GetCircuitDef());
	if (reprDef == nullptr) {
		miRequire = unit->GetCircuitDef()->GetBuildSpeed();
		eiRequire = miRequire / circuit->GetEconomyManager()->GetEcoEM();
	} else {
		const float buildTime = reprDef->GetBuildTime() / facWorkerTime;
		miRequire = reprDef->GetCostM() / buildTime;
		eiRequire = reprDef->GetCostE() / buildTime;
	}
	float miRequireTotal = miRequire;
	float eiRequireTotal = eiRequire;
	unsigned int nanoSize = 0;
	for (auto& kv : nanos) {
		const float mod = kv.first->GetWorkerTime() / facWorkerTime;
		kv.second.incomeMod = mod;
		const float metalUse = miRequire * mod;
		const float energyUse = eiRequire * mod + kv.first->GetUpkeepE();
		miRequireTotal += metalUse;
		eiRequireTotal += energyUse;
		nanoSize += kv.second.units.size();

		for (CCircuitUnit* ass : kv.second.units) {
			SAssistToFactory& af = assists[ass];
			if (af.factories.empty()) {
				af.metalRequire = metalUse;
				af.energyRequire = energyUse;
				metalRequire += metalUse;
				energyRequire += energyUse;
			} else {
				if (af.metalRequire < metalUse) {
					metalRequire += metalUse - af.metalRequire;
					af.metalRequire = metalUse;
				}
				if (af.energyRequire < energyUse) {
					energyRequire += energyUse - af.energyRequire;
					af.energyRequire = energyUse;
				}
			}
			af.factories.insert(unit);
		}
	}

	metalRequire += miRequire;
	energyRequire += eiRequire;

	if (factories.empty()) {
		circuit->GetSetupManager()->SetBasePos(pos);
		circuit->GetMilitaryManager()->MakeBaseDefence(pos);
	}

	auto it = factoryDefs.find(unit->GetCircuitDef()->GetId());
	if (it != factoryDefs.end()) {
		const SFactoryDef& facDef = it->second;
		factories.emplace_back(unit, nanos, nanoSize, facDef.nanoCount, GetFacRoleDef(ROLE_TYPE(BUILDER), facDef),
				miRequire, eiRequire, miRequireTotal, eiRequireTotal);
	} else {
		factories.emplace_back(unit, nanos, nanoSize, 0, nullptr, 0.f, 0.f, 0.f, 0.f);
	}

	if (!factoryData->IsT1Factory(unit->GetCircuitDef())) {
		noT1FacCount++;
	}

	if (unit->GetCircuitDef()->GetMobileId() < 0) {
		validAir.insert(unit);
	}
}

void CFactoryManager::DisableFactory(CCircuitUnit* unit)
{
	std::vector<CRecruitTask*> garbageTasks;
	for (CRecruitTask* task : factoryTasks) {
		if (task->GetAssignees().empty()) {
			garbageTasks.push_back(task);
		}
	}
	for (CRecruitTask* task : garbageTasks) {
		AbortTask(task);
	}

	auto checkBuilderFactory = [this](const int frame) {
		CBuilderManager* builderMgr = this->circuit->GetBuilderManager();
		// check if any factory with builders left
		bool hasBuilder = false;
		for (SFactory& fac : factories) {
			if ((fac.builder != nullptr) && fac.builder->IsAvailable(frame)) {
				hasBuilder = true;
				break;
			}
		}
		if (!hasBuilder) {
			// check queued factories
			std::set<IBuilderTask*> tasks = builderMgr->GetTasks(IBuilderTask::BuildType::FACTORY);
			for (IBuilderTask* task : tasks) {
				auto it = factoryDefs.find(task->GetBuildDef()->GetId());
				if (it != factoryDefs.end()) {
					const SFactoryDef& facDef = it->second;
					CCircuitDef* bdef = GetFacRoleDef(ROLE_TYPE(BUILDER), facDef);
					hasBuilder = ((bdef != nullptr) && bdef->IsAvailable(frame));
					if (hasBuilder) {
						break;
					} else if (task->GetTarget() == nullptr) {
						builderMgr->AbortTask(task);
					}
				}
			}
			if (!hasBuilder) {
				// queue new factory with builder
				CCircuitDef* facDef = GetFactoryToBuild(-RgtVector, true, true);
				if (facDef != nullptr) {
					builderMgr->Enqueue(TaskB::Factory(IBuilderTask::Priority::NOW, facDef, -RgtVector, GetRepresenter(facDef)));
				}
			}
		}
	};

	if (unit->GetTask()->GetType() == IUnitTask::Type::NIL) {
		checkBuilderFactory(circuit->GetLastFrame());
		return;
	}

	for (auto it = factories.begin(); it != factories.end(); ++it) {
		if (it->unit != unit) {
			continue;
		}

		metalRequire -= it->miRequire;
		energyRequire -= it->eiRequire;

		for (auto& kv : it->nanos) {
			for (CCircuitUnit* ass : kv.second.units) {
				SAssistToFactory& af = assists[ass];
				af.factories.erase(unit);
				if (af.factories.empty()) {
					metalRequire -= af.metalRequire;
					energyRequire -= af.energyRequire;
					af.metalRequire = 0.f;
					af.energyRequire = 0.f;
				} else {
					float maxMetalUse = 0.f;
					float maxEnergyUse = 0.f;
					for (CCircuitUnit* fac : af.factories) {
						auto fit = factories.begin();  // FIXME: Nested loops
						while ((fit->unit != fac) && (fit != factories.end())) {
							++fit;
						}
						if (fit != factories.end()) {
							const float mod = fit->nanos[ass->GetCircuitDef()].incomeMod;
							maxMetalUse = std::max(maxMetalUse, fit->miRequire * mod);
							maxEnergyUse = std::max(maxEnergyUse, fit->eiRequire * mod + ass->GetCircuitDef()->GetUpkeepE());
						}
					}
					metalRequire -= af.metalRequire - maxMetalUse;
					energyRequire -= af.energyRequire - maxEnergyUse;
					af.metalRequire = maxMetalUse;
					af.energyRequire = maxEnergyUse;
				}
			}
		}

		*it = factories.back();
		factories.pop_back();
		break;
	}

	if (!factories.empty()) {
		const AIFloat3& pos = factories.front().unit->GetPos(circuit->GetLastFrame());
		circuit->GetSetupManager()->SetBasePos(pos);
	}

	if (!factoryData->IsT1Factory(unit->GetCircuitDef())) {
		noT1FacCount--;
	}

	if (unit->GetCircuitDef()->GetMobileId() < 0) {
		validAir.erase(unit);
	}

	checkBuilderFactory(circuit->GetLastFrame());
}

IUnitTask* CFactoryManager::DefaultMakeTask(CCircuitUnit* unit)
{
	const IUnitTask* task = nullptr;

	if (unit->GetCircuitDef()->IsAssist()) {
		task = CreateAssistTask(unit);

	} else {

		for (const CRecruitTask* candy : factoryTasks) {
			if (candy->CanAssignTo(unit)) {
				task = candy;
				break;
			}
		}

		if (task == nullptr) {
			task = CreateFactoryTask(unit);
		}
	}

	return const_cast<IUnitTask*>(task);  // if nullptr then continue to Wait (or Idle)
}

IUnitTask* CFactoryManager::CreateFactoryTask(CCircuitUnit* unit)
{
	if (unit->GetCircuitDef()->GetMobileId() < 0) {
		if (circuit->GetEnemyManager()->IsAirValid()) {
			if (validAir.find(unit) == validAir.end()) {
				EnableFactory(unit);
			}
		} else {
			if (validAir.find(unit) != validAir.end()) {
				DisableFactory(unit);
			}
			return Enqueue(TaskS::Wait(false, FRAMES_PER_SEC * 10));
		}
	}

	const bool isActive = (noT1FacCount <= 0) || !factoryData->IsT1Factory(unit->GetCircuitDef());

	IUnitTask* task = UpdateBuildPower(unit, isActive);
	if (task != nullptr) {
		return task;
	}

	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	const bool isStalling = economyMgr->IsMetalEmpty() &&
							(economyMgr->GetAvgMetalIncome() * 1.2f < economyMgr->GetMetalPull()) &&
							(metalPull * economyMgr->GetPullMtoS() > circuit->GetBuilderManager()->GetMetalPull());
	const bool isNotReady = !economyMgr->IsExcessed() || isStalling;
	if (isNotReady) {
		return Enqueue(TaskS::Wait(false, FRAMES_PER_SEC * 3));
	}

	task = UpdateFirePower(unit, isActive);
	if (task != nullptr) {
		return task;
	}

	return Enqueue(TaskS::Wait(false, isActive ? (FRAMES_PER_SEC * 3) : (FRAMES_PER_SEC * 10)));
}

IUnitTask* CFactoryManager::CreateAssistTask(CCircuitUnit* unit)
{
	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	bool isMetalEmpty = economyMgr->IsMetalEmpty();
	CAllyUnit* repairTarget = nullptr;
	CAllyUnit* buildTarget = nullptr;
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	float radius = unit->GetCircuitDef()->GetBuildDistance();

	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	CCircuitDef* terraDef = builderMgr->GetTerraDef();
	const float maxCost = MAX_BUILD_SEC * economyMgr->GetAvgMetalIncome() * economyMgr->GetEcoFactor();
	float curCost = std::numeric_limits<float>::max();
	circuit->UpdateFriendlyUnits();
	// NOTE: OOAICallback::GetFriendlyUnitsIn depends on unit's radius
	auto& units = circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius * 0.9f);
	for (Unit* u : units) {
		CAllyUnit* candUnit = circuit->GetFriendlyUnit(u);
		if ((candUnit == nullptr) || (candUnit == unit)
			|| builderMgr->IsReclaimUnit(candUnit)
			|| candUnit->GetCircuitDef()->IsMex())  // FIXME: BA, should be IsT1Mex()
		{
			continue;
		}
		if (u->IsBeingBuilt()) {
			CCircuitDef* cdef = candUnit->GetCircuitDef();
			const float maxHealth = u->GetMaxHealth();
			const float buildTime = cdef->GetBuildTime() * (maxHealth - u->GetHealth()) / maxHealth;
			if (buildTime >= curCost) {
				continue;
			}
			if (IsHighPriority(candUnit) ||
				(!isMetalEmpty && cdef->IsAssistable()) ||
				(*cdef == *terraDef) ||
				(buildTime < maxCost))
			{
				curCost = buildTime;
				buildTarget = candUnit;
			}
		} else if ((repairTarget == nullptr) && (u->GetHealth() < u->GetMaxHealth())) {
			repairTarget = candUnit;
			if (isMetalEmpty) {
				break;
			}
		}
	}
	utils::free(units);
	if (/*!isMetalEmpty && */(buildTarget != nullptr)) {
		// Construction task
		IBuilderTask::Priority priority = buildTarget->GetCircuitDef()->IsMobile() ?
										  IBuilderTask::Priority::HIGH :
										  IBuilderTask::Priority::NORMAL;
		return Enqueue(TaskS::Repair(priority, buildTarget));
	}
	if (repairTarget != nullptr) {
		// Repair task
		return Enqueue(TaskS::Repair(IBuilderTask::Priority::NORMAL, repairTarget));
	}
	if (isMetalEmpty) {
		// Reclaim task
		if (circuit->GetCallback()->IsFeaturesIn(pos, radius) && !builderMgr->IsResurrect(pos, radius)) {
			return Enqueue(TaskS::Reclaim(IBuilderTask::Priority::NORMAL, pos, radius));
		}
	}

	return Enqueue(TaskS::Wait(false, FRAMES_PER_SEC * 3));
}

void CFactoryManager::Watchdog()
{
	ZoneScopedN(__PRETTY_FUNCTION__);

	auto checkIdler = [this](CCircuitUnit* unit) {
		if (unit->GetTask()->GetType() == IUnitTask::Type::PLAYER) {
			return;
		}
		if (!this->circuit->GetCallback()->Unit_HasCommands(unit->GetId())) {
			UnitIdle(unit);
		}
	};

	for (SFactory& fac : factories) {
		checkIdler(fac.unit);
	}

	for (auto& kv : assists) {
		checkIdler(kv.first);
	}
}

void CFactoryManager::SetLastRequiredDef(CCircuitDef::Id facId, CCircuitDef* cdef,
		const std::vector<float>& probs, bool isResp)
{
	SFireDef& lastDef = lastFireDef[facId];
	lastDef.cdef = cdef;
	lastDef.probs = &probs;
	lastDef.buildCount = 0;
	lastDef.isResponse = isResp;
}

std::pair<CCircuitDef*, bool> CFactoryManager::GetLastRequiredDef(CCircuitDef::Id facId,
		const std::vector<float>& probs, const std::function<bool (CCircuitDef*)>& isAvailable)
{
	SFireDef& lastDef = lastFireDef[facId];
	if ((lastDef.cdef != nullptr) && (++lastDef.buildCount < numBatch) && isAvailable(lastDef.cdef)
		&& (!lastDef.isResponse || ((&probs == lastDef.probs) && (circuit->GetMilitaryManager()->RoleProbability(lastDef.cdef) > 0.f))))
	{
		return std::make_pair(lastDef.cdef, lastDef.isResponse);
	}
	return std::make_pair(nullptr, false);
}

CFactoryManager::SRecruitDef CFactoryManager::RequiredFireDef(CCircuitUnit* builder, bool isActive)
{
	auto it = factoryDefs.find(builder->GetCircuitDef()->GetId());
	if (it == factoryDefs.end()) {
		return {-1};
	}
	const SFactoryDef& facDef = it->second;

	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	const int frame = circuit->GetLastFrame();
	const bool isMetalFull = economyMgr->IsMetalFull();
	// spoiled by metal converters, using isEnergyStalling
//	const float energyNet = economyMgr->GetAvgEnergyIncome() - economyMgr->GetEnergyPull();
	const bool isEnergyStalling = economyMgr->IsEnergyStalling();
	const float range = builder->GetCircuitDef()->GetBuildDistance();
	const AIFloat3& pos = builder->GetPos(frame);

	const int iS = terrainMgr->GetSectorIndex(pos);
	auto isEnemyInArea = [&](CCircuitDef* bd) {
		if (isMetalFull || (frame < FRAMES_PER_SEC * 60 * 10)) {  // TODO: Change to minimum required army power
			return true;
		}
		SArea* area;
		bool isValid;
		std::tie(area, isValid) = terrainMgr->GetCurrentMapArea(bd, iS);
		return isValid && ((area == nullptr) || terrainMgr->IsEnemyInArea(area));
	};
	auto isAvailableDef = [&](CCircuitDef* bd) {
		return (((bd->GetCloakCost() < .1f)/* || (energyNet > bd->GetCloakCost())*/ || !isEnergyStalling)
			&& bd->IsAvailable(frame)
			&& (isActive || bd->IsAttrRare())
			&& terrainMgr->CanBeBuiltAt(bd, pos, range)
			&& isEnemyInArea(bd));
	};

	const std::vector<float>& probs = GetFacTierProbs(facDef);
	auto getPriority = [](const bool isResponse) {
		return isResponse ? CRecruitTask::Priority::NORMAL : CRecruitTask::Priority::LOW;
	};

#ifdef FACTORY_CHOICE
	circuit->LOG("---- FACTORY AI = %i | %s | %s | tier%i ----", circuit->GetSkirmishAIId(), builder->GetCircuitDef()->GetDef()->GetName(), unitTypeDbg.c_str(), tierDbg);
#endif
	CCircuitDef* buildDef = nullptr;
	bool isResponse = false;
	std::tie(buildDef, isResponse) = GetLastRequiredDef(it->first, probs, isAvailableDef);
	if ((buildDef != nullptr) && isResponse) {
#ifdef FACTORY_CHOICE
		circuit->LOG("batch %s", buildDef->GetDef()->GetName());
#endif
		return {buildDef->GetId(), getPriority(isResponse)};
	}

	struct SCandidate {
		CCircuitDef* cdef;
		float weight;
		bool isResponse;
	};
	std::vector<SCandidate> candidates;
	candidates.reserve(facDef.buildDefs.size());

	CMilitaryManager* militaryMgr = circuit->GetMilitaryManager();
	const float maxCost = militaryMgr->GetArmyCost();
	float magnitude = 0.f;
	for (unsigned i = 0; i < facDef.buildDefs.size(); ++i) {
		CCircuitDef* bd = facDef.buildDefs[i];
		if (!isAvailableDef(bd)) {
#ifdef FACTORY_CHOICE
			std::string reason;
			if ((bd->GetCloakCost() > .1f)/* && (energyNet < bd->GetCloakCost())*/ && isEnergyStalling) {
				reason = "no energy";
			} else if (!bd->IsAvailable(frame)) {
				reason = "limit exceeded or frame < since";
			} else if (!isActive && !bd->IsAttrRare()) {
				reason = "non-rare from inactive factory";
			} else if (!terrainMgr->CanBeBuiltAt(bd, pos, range)) {
				reason = "can't build unit at unusable map position";
			} else if (!isEnemyInArea(bd)) {
				reason = "no enemies in related map area";
			}
			circuit->LOG("ignore %s | reason = %s", bd->GetDef()->GetName(), reason.c_str());
#endif
			continue;
		}

		if (probs[i] > 0.f) {
			// (probs[i] + response_weight) hints preferable buildDef within same role
			float prob = militaryMgr->RoleProbability(bd) * (probs[i] + reWeight);
			// NOTE: with probs=[n1, n2, n3, n4, n5]
			//       previous response system provided probs2=[0, res(n2), 0, 0, res(n5)]
			//       current is probs2=[n1, res(n2), n3, n4, res(n5)]
			bool isResp = (prob > 0.f) && (bd->GetCostM() <= maxCost);
			isResponse |= isResp;
			prob = isResp ? prob : probs[i];
			candidates.push_back({bd, prob, isResp});
			magnitude += prob;
#ifdef FACTORY_CHOICE
			circuit->LOG("%s | %s | %f", isResp ? "response" : "regular", bd->GetDef()->GetName(), prob);
#endif
		}
	}

	if ((buildDef != nullptr) && !isResponse) {
#ifdef FACTORY_CHOICE
		circuit->LOG("batch %s", buildDef->GetDef()->GetName());
#endif
		return {buildDef->GetId(), getPriority(isResponse)};
	}

	if (magnitude == 0.f) {  // workaround for disabled units
		if (!candidates.empty()) {
			buildDef = candidates[rand() % candidates.size()].cdef;
		}
	} else {
		float dice = (float)rand() / RAND_MAX * magnitude;
		for (const SCandidate& candy : candidates) {
			dice -= candy.weight;
			if (dice < 0.f) {
				buildDef = candy.cdef;
				isResponse = candy.isResponse;
				break;
			}
		}
	}

	SetLastRequiredDef(it->first, buildDef, probs, isResponse);
	if (buildDef != nullptr) {
#ifdef FACTORY_CHOICE
		circuit->LOG("choice = %s", buildDef->GetDef()->GetName());
#endif
		return {buildDef->GetId(), getPriority(isResponse)};
	}
	return {-1};
}

const std::vector<float>& CFactoryManager::GetFacTierProbs(const SFactoryDef& facDef) const
{
	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	const float metalIncome = std::min(economyMgr->GetAvgMetalIncome(), economyMgr->GetAvgEnergyIncome()) * economyMgr->GetEcoFactor();
	const bool isWaterMap = circuit->GetTerrainManager()->IsWaterMap();
	const bool isAir = circuit->GetEnemyManager()->GetEnemyCost(ROLE_TYPE(AIR)) > 1.f;
	const SFactoryDef::Tiers& tiers = isAir ? facDef.airTiers : isWaterMap ? facDef.waterTiers : facDef.landTiers;
#ifdef FACTORY_CHOICE
	unitTypeDbg = isAir ? "air" : isWaterMap ? "water" : "land";
	tierDbg = 0;
#endif
	auto facIt = tiers.begin();
	if ((metalIncome >= facDef.incomes[facIt->first]) && !(facDef.isRequireEnergy && economyMgr->IsEnergyEmpty())) {
		while (facIt != tiers.end()) {
			if (metalIncome < facDef.incomes[facIt->first]) {
				break;
			}
			++facIt;
#ifdef FACTORY_CHOICE
			tierDbg++;
#endif
		}
		if (facIt == tiers.end()) {
			--facIt;
#ifdef FACTORY_CHOICE
			tierDbg--;
#endif
		}
	}
	return facIt->second;
}

CCircuitDef* CFactoryManager::GetFacRoleDef(CCircuitDef::RoleT role, const SFactoryDef& facDef) const
{
	const std::vector<float>& probs = GetFacTierProbs(facDef);

	static std::vector<std::pair<CCircuitDef*, float>> candidates;  // NOTE: micro-opt
//	candidates.reserve(facDef.buildDefs.size());

	const int frame = circuit->GetLastFrame();
	float magnitude = 0.f;
	for (unsigned i = 0; i < facDef.buildDefs.size(); ++i) {
		CCircuitDef* bd = facDef.buildDefs[i];
		if ((bd->GetMainRole() != role) || !bd->IsAvailable(frame)) {
			continue;
		}

		if (probs[i] > 0.f) {
			candidates.push_back(std::make_pair(bd, probs[i]));
			magnitude += probs[i];
		}
	}

	CCircuitDef* buildDef = nullptr;
	if (magnitude == 0.f) {  // workaround for disabled units
		if (!candidates.empty()) {
			buildDef = candidates[rand() % candidates.size()].first;
		}
	} else {
		float dice = (float)rand() / RAND_MAX * magnitude;
		for (auto& pair : candidates) {
			dice -= pair.second;
			if (dice < 0.f) {
				buildDef = pair.first;
				break;
			}
		}
	}
	candidates.clear();

	return buildDef;
}

} // namespace circuit
