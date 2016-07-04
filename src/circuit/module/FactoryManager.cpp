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
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "task/NullTask.h"
#include "task/IdleTask.h"
#include "task/static/RepairTask.h"
#include "task/static/ReclaimTask.h"
#include "unit/FactoryData.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#include "json/json.h"

#include "AIFloat3.h"
#include "OOAICallback.h"
#include "Command.h"
#include "Feature.h"
#include "Log.h"

namespace circuit {

using namespace springai;

CFactoryManager::CFactoryManager(CCircuitAI* circuit)
		: IUnitModule(circuit)
		, factoryPower(.0f)
		, bpRatio(1.f)
		, reWeight(.5f)
		, assistDef(nullptr)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::Watchdog, this),
							FRAMES_PER_SEC * 60,
							circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 11);
	const int interval = 4;
	const int offset = circuit->GetSkirmishAIId() % interval;
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::UpdateIdle, this), interval, offset + 0);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::UpdateAssist, this), interval, offset + 2);

	/*
	 * factory handlers
	 */
	auto factoryCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nullTask->AssignTo(unit);
		}
	};
	auto factoryFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			idleTask->AssignTo(unit);
		} else {
			nullTask->RemoveAssignee(unit);
		}
		this->circuit->AddActionUnit(unit);

		TRY_UNIT(this->circuit, unit,
//			unit->GetUnit()->SetFireState(2);
			unit->GetUnit()->SetRepeat(true);
			unit->GetUnit()->SetIdleMode(0);
		)

		factoryPower += unit->GetCircuitDef()->GetBuildSpeed();

		// check nanos around
		std::set<CCircuitUnit*> nanos;
		float radius = assistDef->GetBuildDistance();
		const AIFloat3& pos = unit->GetPos(this->circuit->GetLastFrame());
		auto units = std::move(this->circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius));
		int nanoId = assistDef->GetId();
		int teamId = this->circuit->GetTeamId();
		for (Unit* nano : units) {
			if (nano == nullptr) {
				continue;
			}
			UnitDef* ndef = nano->GetDef();
			if (ndef->GetUnitDefId() == nanoId && (nano->GetTeam() == teamId) && !nano->IsBeingBuilt()) {
				CCircuitUnit* ass = this->circuit->GetTeamUnit(nano->GetUnitId());
				nanos.insert(ass);

				std::set<CCircuitUnit*>& facs = assists[ass];
				if (facs.empty()) {
					factoryPower += ass->GetCircuitDef()->GetBuildSpeed();
				}
				facs.insert(unit);
			}
			delete ndef;
		}
		utils::free_clear(units);

		auto it = factoryDefs.find(unit->GetCircuitDef()->GetId());
		if (it != factoryDefs.end()) {
			const SFactoryDef& facDef = it->second;
			bool hasBuilder = (facDef.GetRoleDef(CCircuitDef::RoleType::BUILDER) != nullptr);
			factories.emplace_back(unit, nanos, facDef.nanoCount, hasBuilder);
		} else {
			factories.emplace_back(unit, nanos, 0, false);
		}

//		this->circuit->GetSetupManager()->SetBasePos(pos);
	};
	auto factoryIdleHandler = [this](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto factoryDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		// NOTE: Do not del if factory rotation wanted
//		DelFactory(unit->GetCircuitDef());
		auto checkBuilderFactory = [this]() {
			CBuilderManager* builderManager = this->circuit->GetBuilderManager();
			if (!builderManager->GetTasks(IBuilderTask::BuildType::FACTORY).empty()) {
				return;
			}
			// check if any factory with builders left
			bool hasBuilder = false;
			for (SFactory& fac : factories) {
				if (fac.hasBuilder) {
					hasBuilder = true;
					break;
				}
			}
			if (!hasBuilder) {
				CCircuitDef* facDef = GetFactoryToBuild();
				if (facDef != nullptr) {
					builderManager->EnqueueTask(IBuilderTask::Priority::NOW, facDef, -RgtVector,
												IBuilderTask::BuildType::FACTORY);
				}
			}
		};

		if (unit->GetUnit()->IsBeingBuilt()) {  // alternative: task == nullTask
			checkBuilderFactory();
			return;
		}
		factoryPower -= unit->GetCircuitDef()->GetBuildSpeed();
		for (auto it = factories.begin(); it != factories.end(); ++it) {
			if (it->unit != unit) {
				continue;
			}
			for (CCircuitUnit* ass : it->nanos) {
				std::set<CCircuitUnit*>& facs = assists[ass];
				facs.erase(unit);
				if (facs.empty()) {
					factoryPower -= ass->GetCircuitDef()->GetBuildSpeed();
				}
			}
//			factories.erase(it);  // NOTE: micro-opt
			*it = factories.back();
			factories.pop_back();
			break;
		}

		checkBuilderFactory();
	};

	/*
	 * armnanotc handlers
	 */
	auto assistCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nullTask->AssignTo(unit);
		}
	};
	auto assistFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			idleTask->AssignTo(unit);
		} else {
			nullTask->RemoveAssignee(unit);
		}
		this->circuit->AddActionUnit(unit);

		int frame = this->circuit->GetLastFrame();
		const AIFloat3& assPos = unit->GetPos(frame);
		TRY_UNIT(this->circuit, unit,
			unit->GetUnit()->ExecuteCustomCommand(CMD_PRIORITY, {0.0f});
		)

		// check factory nano belongs to
		const float radius = unit->GetCircuitDef()->GetBuildDistance();
		const float sqRadius = SQUARE(radius);
		std::set<CCircuitUnit*>& facs = assists[unit];
		for (SFactory& fac : factories) {
			if (assPos.SqDistance2D(fac.unit->GetPos(frame)) >= sqRadius) {
				continue;
			}
			fac.nanos.insert(unit);
			facs.insert(fac.unit);
		}
		if (!facs.empty()) {
			factoryPower += unit->GetCircuitDef()->GetBuildSpeed();

			bool isInHaven = false;
			for (const AIFloat3& hav : havens) {
				if (assPos.SqDistance2D(hav) < sqRadius) {
					isInHaven = true;
					break;
				}
			}
			if (!isInHaven) {
				havens.push_back(assPos);
				// TODO: Send HavenFinished message?
			}
		}
	};
	auto assistIdleHandler = [this](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto assistDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		if (unit->GetUnit()->IsBeingBuilt()) {  // alternative: task == nullTask
			return;
		}
		const AIFloat3& assPos = unit->GetPos(this->circuit->GetLastFrame());
		const float radius = unit->GetCircuitDef()->GetBuildDistance();
		const float sqRadius = SQUARE(radius);
		for (SFactory& fac : factories) {
			if ((fac.nanos.erase(unit) == 0) || !fac.nanos.empty()) {
				continue;
			}
			auto it = havens.begin();
			while (it != havens.end()) {
				if (it->SqDistance2D(assPos) < sqRadius) {
//					it = havens.erase(it);  // NOTE: micro-opt
					*it = havens.back();
					havens.pop_back();
					// TODO: Send HavenDestroyed message?
				} else {
					++it;
				}
			}
		}
		if (!assists[unit].empty()) {
			factoryPower -= unit->GetCircuitDef()->GetBuildSpeed();
		}
		assists.erase(unit);
	};

	float maxBuildDist = .0f;
	CCircuitDef* assistCandy = nullptr;

	const CCircuitAI::CircuitDefs& allDefs = circuit->GetCircuitDefs();
	for (auto& kv : allDefs) {
		CCircuitDef* cdef = kv.second;
		if (!cdef->IsMobile() && cdef->GetUnitDef()->IsBuilder()) {
			CCircuitDef::Id unitDefId = kv.first;
			if  (!cdef->GetBuildOptions().empty()) {
				createdHandler[unitDefId] = factoryCreatedHandler;
				finishedHandler[unitDefId] = factoryFinishedHandler;
				idleHandler[unitDefId] = factoryIdleHandler;
				destroyedHandler[unitDefId] = factoryDestroyedHandler;
			} else if (maxBuildDist < cdef->GetBuildDistance()) {
				maxBuildDist = cdef->GetBuildDistance();
				createdHandler[unitDefId] = assistCreatedHandler;
				finishedHandler[unitDefId] = assistFinishedHandler;
				idleHandler[unitDefId] = assistIdleHandler;
				destroyedHandler[unitDefId] = assistDestroyedHandler;
				assistCandy = cdef;
			}
		}
	}

	assistDef = assistCandy;

	factoryData = circuit->GetAllyTeam()->GetFactoryData().get();
	ReadConfig();
}

CFactoryManager::~CFactoryManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(factoryTasks);
	utils::free_clear(deleteTasks);
	utils::free_clear(assistTasks);
	utils::free_clear(deleteAssists);
}

int CFactoryManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetCircuitDef()->GetId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	if (builder == nullptr) {
		return 0; //signaling: OK
	}

	IUnitTask* task = builder->GetTask();
	if (task->GetType() != IUnitTask::Type::FACTORY) {
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
	auto itre = repairedUnits.find(unit->GetId());
	if (itre != repairedUnits.end()) {
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

int CFactoryManager::UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		AbortTask(iter->second);
	}
	auto itre = repairedUnits.find(unit->GetId());
	if (itre != repairedUnits.end()) {
		AbortTask(itre->second);
	}

	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

CRecruitTask* CFactoryManager::EnqueueTask(CRecruitTask::Priority priority,
										   CCircuitDef* buildDef,
										   const AIFloat3& position,
										   CRecruitTask::RecruitType type,
										   float radius)
{
	CRecruitTask* task = new CRecruitTask(this, priority, buildDef, position, type, radius);
	factoryTasks.insert(task);
	return task;
}

IBuilderTask* CFactoryManager::EnqueueReclaim(IBuilderTask::Priority priority,
											  const springai::AIFloat3& position,
											  float radius,
											  int timeout)
{
	IBuilderTask* task = new CSReclaimTask(this, priority, position, .0f, timeout, radius);
	assistTasks.insert(task);
	return task;
}

IBuilderTask* CFactoryManager::EnqueueRepair(IBuilderTask::Priority priority,
											 CCircuitUnit* target)
{
	auto it = repairedUnits.find(target->GetId());
	if (it != repairedUnits.end()) {
		return it->second;
	}
	IBuilderTask* task = new CSRepairTask(this, priority, target);
	assistTasks.insert(task);
	repairedUnits[target->GetId()] = task;
	return task;
}

void CFactoryManager::DequeueTask(IUnitTask* task, bool done)
{
	auto itf = factoryTasks.find(static_cast<CRecruitTask*>(task));
	if (itf != factoryTasks.end()) {
		unfinishedUnits.erase(static_cast<CRecruitTask*>(task)->GetTarget());
		factoryTasks.erase(itf);
		task->Close(done);
		deleteTasks.insert(static_cast<CRecruitTask*>(task));
		return;
	}
	auto ita = assistTasks.find(static_cast<IBuilderTask*>(task));
	if (ita != assistTasks.end()) {
		if (static_cast<IBuilderTask*>(task)->GetBuildType() == IBuilderTask::BuildType::REPAIR) {
			repairedUnits.erase(static_cast<CSRepairTask*>(task)->GetTargetId());
		}
		assistTasks.erase(ita);
		task->Close(done);
		deleteAssists.insert(static_cast<IBuilderTask*>(task));
	}
}

IUnitTask* CFactoryManager::MakeTask(CCircuitUnit* unit)
{
	IUnitTask* task = nullptr;

	if (unit->GetCircuitDef() == assistDef) {
		task = CreateAssistTask(unit);

	} else {

		decltype(factoryTasks)::iterator iter = factoryTasks.begin();
		for (; iter != factoryTasks.end(); ++iter) {
			if ((*iter)->CanAssignTo(unit)) {
				task = static_cast<CRecruitTask*>(*iter);
				break;
			}
		}

		if (task == nullptr) {
			task = CreateFactoryTask(unit);
		}
	}

	return task;  // if nullptr then continue to Wait (or Idle)
}

void CFactoryManager::AbortTask(IUnitTask* task)
{
	DequeueTask(task, false);
}

void CFactoryManager::DoneTask(IUnitTask* task)
{
	DequeueTask(task, true);
}

void CFactoryManager::FallbackTask(CCircuitUnit* unit)
{
}

CCircuitUnit* CFactoryManager::NeedUpgrade()
{
	unsigned facSize = factories.size();
	for (auto it = factories.rbegin(); it != factories.rend(); ++it) {
		SFactory& fac = *it;
		if (fac.nanos.size() < facSize * fac.weight) {
			return fac.unit;
		}
	}
	return nullptr;
}

CCircuitUnit* CFactoryManager::GetRandomFactory(CCircuitDef* buildDef)
{
	static std::vector<CCircuitUnit*> facs;  // NOTE: micro-opt
//	facs.reserve(factories.size());
	for (SFactory& fac : factories) {
		if (fac.unit->GetCircuitDef()->CanBuild(buildDef)) {
			facs.push_back(fac.unit);
		}
	}
	if (facs.empty()) {
		return nullptr;
	}
	auto it = facs.begin();
	std::advance(it, rand() % facs.size());
	CCircuitUnit* result = *it;
	facs.clear();
	return result;
}

CCircuitDef* CFactoryManager::GetClosestDef(AIFloat3& position, CCircuitDef::RoleType role)
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	terrainManager->CorrectPosition(position);
	int iS = terrainManager->GetSectorIndex(position);
	CCircuitDef* roleDef = nullptr;
	float minSqDist = std::numeric_limits<float>::max();
	int frame = circuit->GetLastFrame();
	for (SFactory& fac : factories) {
		STerrainMapArea* area = fac.unit->GetArea();
		if ((area == nullptr) || (area->sector.find(iS) == area->sector.end())) {
			continue;
		}
		const AIFloat3& facPos = fac.unit->GetPos(frame);
		float sqDist = position.SqDistance2D(facPos);
		if (minSqDist < sqDist) {
			continue;
		}
		const SFactoryDef& facDef = factoryDefs.find(fac.unit->GetCircuitDef()->GetId())->second;
		CCircuitDef* cdef = facDef.GetRoleDef(role);
		if (cdef != nullptr) {
			roleDef = cdef;
			minSqDist = sqDist;
			position = facPos;
		}
	}
	return roleDef;
}

AIFloat3 CFactoryManager::GetClosestHaven(CCircuitUnit* unit) const
{
	if (havens.empty()) {
		return -RgtVector;
	}
	float metric = std::numeric_limits<float>::max();
	const AIFloat3& position = unit->GetPos(circuit->GetLastFrame());
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	auto it = havens.begin(), havIt = havens.end();
	for (; it != havens.end(); ++it) {
		if (!terrainManager->CanMoveToPos(unit->GetArea(), *it)) {
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

CRecruitTask* CFactoryManager::UpdateBuildPower(CCircuitUnit* unit)
{
	if (!CanEnqueueTask()) {
		return nullptr;
	}

	CEconomyManager* economyManager = circuit->GetEconomyManager();
	float metalIncome = std::min(economyManager->GetAvgMetalIncome(), economyManager->GetAvgEnergyIncome());
	if ((circuit->GetBuilderManager()->GetBuilderPower() >= metalIncome * bpRatio) || (rand() >= RAND_MAX / 2)) {
		return nullptr;
	}

	auto it = factoryDefs.find(unit->GetCircuitDef()->GetId());
	if (it == factoryDefs.end()) {
		return nullptr;
	}
	CCircuitDef* buildDef = it->second.GetRoleDef(CCircuitDef::RoleType::BUILDER);
	if ((buildDef == nullptr) || !buildDef->IsAvailable()) {
		return nullptr;
	}

	CCircuitUnit* factory = GetRandomFactory(buildDef);
	if (factory != nullptr) {
		const AIFloat3& buildPos = factory->GetPos(circuit->GetLastFrame());
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		float radius = std::max(terrainManager->GetTerrainWidth(), terrainManager->GetTerrainHeight()) / 4;
		return EnqueueTask(CRecruitTask::Priority::NORMAL, buildDef, buildPos, CRecruitTask::RecruitType::BUILDPOWER, radius);
	}

	return nullptr;
}

CRecruitTask* CFactoryManager::UpdateFirePower(CCircuitUnit* unit)
{
	if (!CanEnqueueTask()) {
		return nullptr;
	}

	auto it = factoryDefs.find(unit->GetCircuitDef()->GetId());
	if (it == factoryDefs.end()) {
		return nullptr;
	}
	const SFactoryDef& facDef = it->second;

	CMilitaryManager* militaryManager = circuit->GetMilitaryManager();
	CCircuitDef* buildDef/* = nullptr*/;

	CEconomyManager* em = circuit->GetEconomyManager();
	const float metalIncome = std::min(em->GetAvgMetalIncome(), em->GetAvgEnergyIncome()) * em->GetEcoFactor();
	bool isWaterMap = circuit->GetTerrainManager()->GetPercentLand() < 40.0;
	const SFactoryDef::Tiers& tiers = isWaterMap ? facDef.waterTiers : facDef.landTiers;
	auto facIt = tiers.begin();
	if ((metalIncome >= facDef.incomes[facIt->first]) && !(facDef.isRequireEnergy && em->IsEnergyEmpty())) {
		while (facIt != tiers.end()) {
			if (metalIncome < facDef.incomes[facIt->first]) {
				break;
			}
			++facIt;
		}
		if (facIt == tiers.end()) {
			--facIt;
		}
	}
	const std::vector<float>& probs = facIt->second;

	static std::vector<std::pair<CCircuitDef*, float>> candidates;  // NOTE: micro-opt
//	candidates.reserve(facDef.buildDefs.size());
	float magnitude = 0.f;
	for (unsigned i = 0; i < facDef.buildDefs.size(); ++i) {
		CCircuitDef* bd = facDef.buildDefs[i];
		// (probs[i] + response_weight) hints preferable buildDef within same role
		float prob = militaryManager->RoleProbability(bd) * (probs[i] + reWeight);
		if (prob > 0.f) {
			candidates.push_back(std::make_pair(bd, prob));
			magnitude += prob;
		}
	}

	if (!candidates.empty()) {
		buildDef = candidates.front().first;
		float dice = (float)rand() / RAND_MAX * magnitude;
		float total = .0f;
		for (auto& pair : candidates) {
			total += pair.second;
			if (dice < total) {
				buildDef = pair.first;
				break;
			}
		}
		candidates.clear();
	} else {
		unsigned choice = 0;
		float dice = (float)rand() / RAND_MAX;
		float total = .0f;
		for (unsigned i = 0; i < probs.size(); ++i) {
			total += probs[i];
			if (dice < total) {
				choice = i;
				break;
			}
		}
		buildDef = facDef.buildDefs[choice];
	}

	if (/*(buildDef != nullptr) && */buildDef->IsAvailable()) {
		const AIFloat3& buildPos = unit->GetPos(circuit->GetLastFrame());
		UnitDef* def = unit->GetCircuitDef()->GetUnitDef();
		float radius = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE * 4;
		// FIXME CCircuitDef::RoleType <-> CRecruitTask::RecruitType relations
		return EnqueueTask(CRecruitTask::Priority::NORMAL, buildDef, buildPos, CRecruitTask::RecruitType::FIREPOWER, radius);
	}

	return nullptr;
}

CCircuitDef* CFactoryManager::GetFactoryToBuild(bool isStart)
{
	return factoryData->GetFactoryToBuild(circuit, isStart);
}

void CFactoryManager::AddFactory(CCircuitDef* cdef)
{
	factoryData->AddFactory(cdef);
}

void CFactoryManager::DelFactory(CCircuitDef* cdef)
{
	factoryData->DelFactory(cdef);
}

CCircuitDef* CFactoryManager::GetRoleDef(CCircuitDef* facDef, CCircuitDef::RoleType role) const
{
	auto it = factoryDefs.find(facDef->GetId());
	return (it != factoryDefs.end()) ? it->second.GetRoleDef(role) : nullptr;
}

CCircuitDef* CFactoryManager::GetLandDef(CCircuitDef* facDef) const
{
	auto it = factoryDefs.find(facDef->GetId());
	return (it != factoryDefs.end()) ? it->second.landDef : nullptr;
}

CCircuitDef* CFactoryManager::GetWaterDef(CCircuitDef* facDef) const
{
	auto it = factoryDefs.find(facDef->GetId());
	return (it != factoryDefs.end()) ? it->second.waterDef : nullptr;
}

void CFactoryManager::ReadConfig()
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();

	/*
	 * Roles, attributes and retreat
	 */
	std::map<CCircuitDef::RoleType, std::set<CCircuitDef::Id>> roleDefs;
	std::map<const char*, CCircuitDef::RoleType, cmp_str> roleNames = {
		{"builder",    CCircuitDef::RoleType::BUILDER},
		{"scout",      CCircuitDef::RoleType::SCOUT},
		{"raider",     CCircuitDef::RoleType::RAIDER},
		{"riot",       CCircuitDef::RoleType::RIOT},
		{"assault",    CCircuitDef::RoleType::ASSAULT},
		{"skirmish",   CCircuitDef::RoleType::SKIRM},
		{"artillery",  CCircuitDef::RoleType::ARTY},
		{"anti_air",   CCircuitDef::RoleType::AA},
		{"anti_heavy", CCircuitDef::RoleType::AH},
		{"bomber",     CCircuitDef::RoleType::BOMBER},
		{"support",    CCircuitDef::RoleType::SUPPORT},
		{"mine",       CCircuitDef::RoleType::MINE},
		{"transport",  CCircuitDef::RoleType::TRANS},
		{"air",        CCircuitDef::RoleType::AIR},
		{"static",     CCircuitDef::RoleType::STATIC},
		{"heavy",      CCircuitDef::RoleType::HEAVY},
	};
	std::map<const char*, CCircuitDef::AttrType, cmp_str> attrNames = {
		{"melee",     CCircuitDef::AttrType::MELEE},
		{"siege",     CCircuitDef::AttrType::SIEGE},
		{"hold_fire", CCircuitDef::AttrType::HOLD_FIRE},
		{"boost",     CCircuitDef::AttrType::BOOST},
		{"no_jump",   CCircuitDef::AttrType::NO_JUMP},
		{"stockpile", CCircuitDef::AttrType::STOCK},
		{"no_strafe", CCircuitDef::AttrType::NO_STRAFE},
	};
	const Json::Value& behaviours = root["behaviour"];
	for (const std::string& defName : behaviours.getMemberNames()) {
		CCircuitDef* cdef = circuit->GetCircuitDef(defName.c_str());
		if (cdef == nullptr) {
			continue;
		}

		// Auto-assign roles
		auto setRoles = [cdef](CCircuitDef::RoleType type) {
			cdef->SetMainRole(type);
			cdef->SetEnemyRole(type);
			cdef->AddRole(type);
		};
		if (cdef->IsAbleToFly()) {
			setRoles(CCircuitDef::RoleType::AIR);
		} else if (!cdef->IsMobile() && cdef->IsAttacker() && cdef->HasAntiLand()) {
			setRoles(CCircuitDef::RoleType::STATIC);
		} else if (cdef->GetUnitDef()->IsBuilder() && !cdef->GetBuildOptions().empty()) {
			setRoles(CCircuitDef::RoleType::BUILDER);
		}

		// Read roles from config
		const Json::Value& behaviour = behaviours[defName];
		const Json::Value& role = behaviour["role"];
		if (role.empty()) {
			circuit->LOG("CONFIG %s: '%s' has no role", cfgName.c_str(), defName.c_str());
			continue;
		}

		const char* mainName = role[0].asCString();
		auto it = roleNames.find(mainName);
		if (it == roleNames.end()) {
			circuit->LOG("CONFIG %s: %s has unknown main role '%s'", cfgName.c_str(), defName.c_str(), mainName);
			continue;
		}
		cdef->SetMainRole(it->second);
		cdef->AddRole(it->second);
		roleDefs[it->second].insert(cdef->GetId());

		const Json::Value& enemyRole = role[1];
		if (enemyRole.isNull()) {
			cdef->SetEnemyRole(it->second);
		} else {
			const char* enemyName = enemyRole.asCString();
			it = roleNames.find(enemyName);
			if (it == roleNames.end()) {
				circuit->LOG("CONFIG %s: %s has unknown enemy role '%s'", cfgName.c_str(), defName.c_str(), enemyName);
				continue;
			}
			cdef->SetEnemyRole(it->second);
			cdef->AddRole(it->second);
		}

		// Read optional roles and attributes
		const Json::Value& attributes = behaviour["attribute"];
		for (const Json::Value& attr : attributes) {
			const std::string& attrName = attr.asString();
			it = roleNames.find(attrName.c_str());
			if (it == roleNames.end()) {
				auto it = attrNames.find(attrName.c_str());
				if (it == attrNames.end()) {
					circuit->LOG("CONFIG %s: %s has unknown attribute '%s'", cfgName.c_str(), defName.c_str(), attrName.c_str());
					continue;
				} else {
					cdef->AddAttribute(it->second);
				}
			} else {
				cdef->AddRole(it->second);
				roleDefs[it->second].insert(cdef->GetId());
			}
		}

		const Json::Value& limit = behaviour["limit"];
		if (!limit.isNull()) {
			cdef->SetMaxThisUnit(limit.asUInt());
		}

		cdef->SetRetreat(behaviour.get("retreat", cdef->GetRetreat()).asFloat());
	}

	/*
	 * Factories
	 */
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	const Json::Value& factories = root["factory"];
	for (const std::string& fac : factories.getMemberNames()) {
		CCircuitDef* cdef = circuit->GetCircuitDef(fac.c_str());
		if (cdef == nullptr) {
			continue;
		}
		const Json::Value& factory = factories[fac];
		SFactoryDef facDef;

		const std::unordered_set<CCircuitDef::Id>& options = cdef->GetBuildOptions();
		std::vector<CCircuitDef::RoleType> facRoles = {  // NOTE: used to create tasks on Event (like DefendTask), fix/improve
			CCircuitDef::RoleType::BUILDER,
			CCircuitDef::RoleType::RIOT,
			CCircuitDef::RoleType::ARTY,
			CCircuitDef::RoleType::AA,
		};

		facDef.roleDefs.resize(static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_), nullptr);
		for (const CCircuitDef::RoleType type : facRoles) {
			float minCost = std::numeric_limits<float>::max();
			CCircuitDef* rdef = nullptr;
			const std::set<CCircuitDef::Id>& defIds = roleDefs[type];
			for (const CCircuitDef::Id bid : defIds) {
				if (options.find(bid) == options.end()) {
					continue;
				}
				CCircuitDef* tdef = circuit->GetCircuitDef(bid);
				if (minCost > tdef->GetCost()) {
					minCost = tdef->GetCost();
					rdef = tdef;
				}
			}
			facDef.roleDefs[static_cast<CCircuitDef::RoleT>(type)] = rdef;
		}

		facDef.isRequireEnergy = factory.get("require_energy", false).asBool();

		const Json::Value& items = factory["unit_def"];
		const Json::Value& tiers = factory["income_tier"];
		facDef.buildDefs.reserve(items.size());
		const unsigned tierSize = tiers.size();
		facDef.incomes.reserve(tierSize);

		CCircuitDef* landDef = nullptr;
		CCircuitDef* waterDef = nullptr;
		float landSize = std::numeric_limits<float>::max();
		float waterSize = std::numeric_limits<float>::max();

		for (unsigned i = 0; i < items.size(); ++i) {
			CCircuitDef* udef = circuit->GetCircuitDef(items[i].asCString());
			if (udef == nullptr) {
				continue;
			}
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
			STerrainMapArea* area = terrainManager->GetMobileTypeById(udef->GetMobileId())->areaLargest;
			if (area == nullptr) {
				continue;
			}
			if ((area->mobileType->maxElevation > -SQUARE_SIZE * 5) && (landSize > area->percentOfMap)) {
				landSize = area->percentOfMap;
				landDef = udef;
			}
			if (((area->mobileType->minElevation < SQUARE_SIZE * 5) || udef->IsFloater()) && (waterSize > area->percentOfMap)) {
				waterSize = area->percentOfMap;
				waterDef = udef;
			}
		}
		if (facDef.buildDefs.empty()) {
			continue;  // ignore empty factory
		}
		facDef.landDef = landDef;
		facDef.waterDef = waterDef;

		auto fillProbs = [this, &cfgName, &facDef, &fac, &factory](unsigned i, const char* type, SFactoryDef::Tiers& tiers) {
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
			for (unsigned j = 0; j < facDef.buildDefs.size(); ++j) {
				const float p = tier[j].asFloat();
				sum += p;
				probs.push_back(p);
			}
			if (fabs(sum - 1.0f) > 0.0001f) {
				circuit->LOG("CONFIG %s: %s's %s_tier%i total probability = %f", cfgName.c_str(), fac.c_str(), type, i, sum);
			}
			return true;
		};
		unsigned i = 0;
		for (; i < tierSize; ++i) {
			facDef.incomes.push_back(tiers[i].asFloat());
			fillProbs(i, "land", facDef.landTiers);
			fillProbs(i, "water", facDef.waterTiers);
		}
		fillProbs(i, "land", facDef.landTiers);
		fillProbs(i, "water", facDef.waterTiers);

		if (facDef.incomes.empty()) {
			facDef.incomes.push_back(std::numeric_limits<float>::max());
		}
		if (facDef.landTiers.empty()) {
			if (!facDef.waterTiers.empty()) {
				facDef.landTiers = facDef.waterTiers;
			} else {
				facDef.landTiers[0];  // create empty tier
			}
		}
		if (facDef.waterTiers.empty()) {
			facDef.waterTiers = facDef.landTiers;
		}

		facDef.nanoCount = factory.get("caretaker", 1).asUInt();

		factoryDefs[cdef->GetId()] = facDef;
	}

	bpRatio = factories.get("_buildpower_", 1.f).asFloat();
	reWeight = root["response"].get("_weight_", .5f).asFloat();
}

IUnitTask* CFactoryManager::CreateFactoryTask(CCircuitUnit* unit)
{
	IUnitTask* task = UpdateBuildPower(unit);
	if (task != nullptr) {
		return task;
	}

	task = UpdateFirePower(unit);
	if (task != nullptr) {
		return task;
	}

	return nullTask;
}

IBuilderTask* CFactoryManager::CreateAssistTask(CCircuitUnit* unit)
{
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	bool isMetalEmpty = economyManager->IsMetalEmpty();
	CCircuitUnit* repairTarget = nullptr;
	CCircuitUnit* buildTarget = nullptr;
	bool isBuildMobile = true;
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	float radius = unit->GetCircuitDef()->GetBuildDistance();

	float maxCost = MAX_BUILD_SEC * economyManager->GetAvgMetalIncome() * economyManager->GetEcoFactor();
	CCircuitDef* terraDef = circuit->GetBuilderManager()->GetTerraDef();
	circuit->UpdateFriendlyUnits();
	// NOTE: OOAICallback::GetFriendlyUnitsIn depends on unit's radius
	auto units = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius * 0.9f));
	for (Unit* u : units) {
		CCircuitUnit* candUnit = circuit->GetFriendlyUnit(u);
		if (candUnit == nullptr) {
			continue;
		}
		if (u->IsBeingBuilt()) {
			CCircuitDef* cdef = candUnit->GetCircuitDef();
			if (isBuildMobile && (!isMetalEmpty || (*cdef == *terraDef) || (cdef->GetCost() < maxCost))) {
				isBuildMobile = candUnit->GetCircuitDef()->IsMobile();
				buildTarget = candUnit;
			}
		} else if ((repairTarget == nullptr) && (u->GetHealth() < u->GetMaxHealth())) {
			repairTarget = candUnit;
			if (isMetalEmpty) {
				break;
			}
		}
	}
	utils::free_clear(units);
	if (!isMetalEmpty && (buildTarget != nullptr)) {
		// Construction task
		IBuilderTask::Priority priority = isBuildMobile ? IBuilderTask::Priority::HIGH : IBuilderTask::Priority::NORMAL;
		return EnqueueRepair(priority, buildTarget);
	}
	if (repairTarget != nullptr) {
		// Repair task
		return EnqueueRepair(IBuilderTask::Priority::NORMAL, repairTarget);
	}
	if (isMetalEmpty) {
		// Reclaim task
		auto features = std::move(circuit->GetCallback()->GetFeaturesIn(pos, radius));
		if (!features.empty()) {
			utils::free_clear(features);
			return EnqueueReclaim(IBuilderTask::Priority::NORMAL, pos, radius);
		}
	}

	return nullptr;
}

void CFactoryManager::Watchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	auto checkIdler = [this](CCircuitUnit* unit) {
		auto commands = std::move(unit->GetUnit()->GetCurrentCommands());
		if (commands.empty()) {
			UnitIdle(unit);
		}
		utils::free_clear(commands);
	};

	for (SFactory& fac : factories) {
		checkIdler(fac.unit);
	}

	for (auto& kv : assists) {
		checkIdler(kv.first);
	}
}

void CFactoryManager::UpdateIdle()
{
	idleTask->Update();
}

void CFactoryManager::UpdateAssist()
{
	utils::free_clear(deleteTasks);
	if (!deleteAssists.empty()) {
		for (IBuilderTask* task : deleteAssists) {
			updateAssists.erase(task);
			delete task;
		}
		deleteAssists.clear();
	}

	auto it = updateAssists.begin();
	unsigned int i = 0;
	while (it != updateAssists.end()) {
		(*it)->Update();

		it = updateAssists.erase(it);
		if (++i >= updateSlice) {
			break;
		}
	}

	if (updateAssists.empty()) {
		updateAssists = assistTasks;
		updateSlice = updateAssists.size() / TEAM_SLOWUPDATE_RATE;
	}
}

} // namespace circuit
