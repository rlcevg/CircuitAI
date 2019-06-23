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
#include "task/NilTask.h"
#include "task/IdleTask.h"
#include "task/static/WaitTask.h"
#include "task/static/RepairTask.h"
#include "task/static/ReclaimTask.h"
#include "unit/FactoryData.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#include "json/json.h"

#include "AIFloat3.h"
#include "OOAICallback.h"
#include "AISCommands.h"
#include "Command.h"
#include "Feature.h"
#include "Log.h"

namespace circuit {

using namespace springai;

CFactoryManager::CFactoryManager(CCircuitAI* circuit)
		: IUnitModule(circuit)
		, updateIterator(0)
		, factoryPower(.0f)
		, assistDef(nullptr)
		, bpRatio(1.f)
		, reWeight(.5f)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::Watchdog, this),
							FRAMES_PER_SEC * 60,
							circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 11);
	scheduler->RunTaskAt(std::make_shared<CGameTask>(&CFactoryManager::Init, this));

	/*
	 * factory handlers
	 */
	auto factoryCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nilTask->AssignTo(unit);
			this->circuit->AddActionUnit(unit);
		}
	};
	auto factoryFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			this->circuit->AddActionUnit(unit);
		}
		idleTask->AssignTo(unit);

		TRY_UNIT(this->circuit, unit,
			unit->GetUnit()->SetRepeat(false);
			unit->GetUnit()->SetIdleMode(0);
		)

		EnableFactory(unit);
	};
	auto factoryIdleHandler = [](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto factoryDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		// NOTE: Do not del if factory rotation wanted
//		DelFactory(unit->GetCircuitDef());
		DisableFactory(unit);
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
		idleTask->AssignTo(unit);

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
			factoryPower += unit->GetBuildSpeed();

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
	auto assistIdleHandler = [](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto assistDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
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
			factoryPower -= unit->GetBuildSpeed();
		}
		assists.erase(unit);
	};

	float maxBuildDist = SQUARE_SIZE * 2;
	CCircuitDef* commDef = circuit->GetSetupManager()->GetCommChoice();

	const CCircuitAI::CircuitDefs& allDefs = circuit->GetCircuitDefs();
	for (auto& kv : allDefs) {
		CCircuitDef* cdef = kv.second;
		UnitDef* def = cdef->GetUnitDef();
		if (!cdef->IsMobile() && def->IsBuilder()) {
			CCircuitDef::Id unitDefId = kv.first;
			if  (!cdef->GetBuildOptions().empty()) {
				createdHandler[unitDefId] = factoryCreatedHandler;
				finishedHandler[unitDefId] = factoryFinishedHandler;
				idleHandler[unitDefId] = factoryIdleHandler;
				destroyedHandler[unitDefId] = factoryDestroyedHandler;
			} else if (maxBuildDist < cdef->GetBuildDistance() &&
				(std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE < cdef->GetBuildDistance()))
			{
				maxBuildDist = cdef->GetBuildDistance();
				createdHandler[unitDefId] = assistCreatedHandler;
				finishedHandler[unitDefId] = assistFinishedHandler;
				idleHandler[unitDefId] = assistIdleHandler;
				destroyedHandler[unitDefId] = assistDestroyedHandler;
				if (commDef->CanBuild(cdef)) {
					assistDef = cdef;
				}
			}
		}

		// Auto-assign roles
		auto setRoles = [cdef](CCircuitDef::RoleType type) {
			cdef->SetMainRole(type);
			cdef->AddEnemyRole(type);
			cdef->AddRole(type);
		};
		if (cdef->IsAbleToFly()) {
			setRoles(CCircuitDef::RoleType::AIR);
		} else if (!cdef->IsMobile() && cdef->IsAttacker() && cdef->HasAntiLand()) {
			setRoles(CCircuitDef::RoleType::STATIC);
		} else if (cdef->GetUnitDef()->IsBuilder() && !cdef->GetBuildOptions().empty() && !cdef->IsRoleComm()) {
			setRoles(CCircuitDef::RoleType::BUILDER);
		}
		if (cdef->IsRoleComm()) {
			// NOTE: Omit AddRole to exclude commanders from response
			cdef->SetMainRole(CCircuitDef::RoleType::BUILDER);
			cdef->AddEnemyRole(CCircuitDef::RoleType::COMM);
		}
	}

	ReadConfig();

	if (assistDef == nullptr) {
		assistDef = circuit->GetEconomyManager()->GetDefaultDef();
	}

	factoryData = circuit->GetAllyTeam()->GetFactoryData().get();
}

CFactoryManager::~CFactoryManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(updateTasks);
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

	if (builder == nullptr) {
		return 0; //signaling: OK
	}

	IUnitTask* task = builder->GetTask();
	if ((task == nullptr) || task->GetType() != IUnitTask::Type::FACTORY) {
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
	factoryTasks.push_back(task);
	updateTasks.push_back(task);
	return task;
}

IUnitTask* CFactoryManager::EnqueueWait(bool stop, int timeout)
{
	CSWaitTask* task = new CSWaitTask(this, stop, timeout);
	updateTasks.push_back(task);
	return task;
}

IBuilderTask* CFactoryManager::EnqueueReclaim(IBuilderTask::Priority priority,
											  const springai::AIFloat3& position,
											  float radius,
											  int timeout)
{
	IBuilderTask* task = new CSReclaimTask(this, priority, position, .0f, timeout, radius);
	updateTasks.push_back(task);
	return task;
}

IBuilderTask* CFactoryManager::EnqueueRepair(IBuilderTask::Priority priority,
											 CAllyUnit* target)
{
	auto it = repairedUnits.find(target->GetId());
	if (it != repairedUnits.end()) {
		return it->second;
	}
	IBuilderTask* task = new CSRepairTask(this, priority, target);
	updateTasks.push_back(task);
	repairedUnits[target->GetId()] = task;
	return task;
}

void CFactoryManager::DequeueTask(IUnitTask* task, bool done)
{
	if (task->GetType() == IUnitTask::Type::FACTORY) {
		switch (static_cast<IBuilderTask*>(task)->GetBuildType()) {
			case IBuilderTask::BuildType::RECRUIT: {
				auto it = std::find(factoryTasks.begin(), factoryTasks.end(), task);
				if (it != factoryTasks.end()) {
					factoryTasks.erase(it);
				}
				unfinishedUnits.erase(static_cast<CRecruitTask*>(task)->GetTarget());
			} break;
			case IBuilderTask::BuildType::REPAIR: {
				repairedUnits.erase(static_cast<CSRepairTask*>(task)->GetTargetId());
			} break;
			default: break;  // RECLAIM
		}
	}  // WAIT
	task->Dead();
	task->Close(done);
}

IUnitTask* CFactoryManager::MakeTask(CCircuitUnit* unit)
{
	const IUnitTask* task = nullptr;

	if (unit->GetCircuitDef() == assistDef) {
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
	const int frame = circuit->GetLastFrame();
	unsigned facSize = factories.size();
	for (auto itF = factories.rbegin(); itF != factories.rend(); ++itF) {
		SFactory& fac = *itF;

		auto itD = factoryDefs.find(fac.unit->GetCircuitDef()->GetId());
		if (itD != factoryDefs.end()) {
			const SFactoryDef& facDef = itD->second;

			for (CCircuitDef* cdef : facDef.buildDefs) {
				if (cdef->IsAvailable(frame)) {
					if (fac.nanos.size() < facSize * fac.weight) {
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

CCircuitUnit* CFactoryManager::GetClosestFactory(AIFloat3 position)
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
//	CTerrainManager::CorrectPosition(position);
	int iS = terrainManager->GetSectorIndex(position);
	CCircuitUnit* factory = nullptr;
	float minSqDist = std::numeric_limits<float>::max();
	const int frame = circuit->GetLastFrame();
	for (SFactory& fac : factories) {
		STerrainMapArea* area = fac.unit->GetArea();
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
	return factory;
}

//CCircuitDef* CFactoryManager::GetClosestDef(AIFloat3& position, CCircuitDef::RoleType role)
//{
//	CTerrainManager* terrainManager = circuit->GetTerrainManager();
//	CTerrainManager::CorrectPosition(position);
//	int iS = terrainManager->GetSectorIndex(position);
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
//		CCircuitDef* cdef = facDef.GetRoleDef(role);
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

CRecruitTask* CFactoryManager::UpdateBuildPower(CCircuitUnit* unit)
{
	if (!CanEnqueueTask()) {
		return nullptr;
	}

	CEconomyManager* economyManager = circuit->GetEconomyManager();
	const float metalIncome = std::min(economyManager->GetAvgMetalIncome(), economyManager->GetAvgEnergyIncome());
	if ((circuit->GetBuilderManager()->GetBuildPower() >= metalIncome * bpRatio) || (rand() >= RAND_MAX / 2)) {
		return nullptr;
	}

	auto it = factoryDefs.find(unit->GetCircuitDef()->GetId());
	if (it == factoryDefs.end()) {
		return nullptr;
	}
	CCircuitDef* buildDef = it->second.GetRoleDef(CCircuitDef::RoleType::BUILDER);

	if ((buildDef != nullptr) && buildDef->IsAvailable(circuit->GetLastFrame())) {
		const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		if (!terrainManager->CanBeBuiltAt(buildDef, pos, unit->GetCircuitDef()->GetBuildDistance())) {
			return nullptr;
		}
		float radius = std::max(terrainManager->GetTerrainWidth(), terrainManager->GetTerrainHeight()) / 4;
		return EnqueueTask(CRecruitTask::Priority::NORMAL, buildDef, pos, CRecruitTask::RecruitType::BUILDPOWER, radius);
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
	CCircuitDef* buildDef = nullptr;

	CEconomyManager* economyManager = circuit->GetEconomyManager();
	const float metalIncome = std::min(economyManager->GetAvgMetalIncome(), economyManager->GetAvgEnergyIncome()) * economyManager->GetEcoFactor();
	const bool isWaterMap = circuit->GetTerrainManager()->IsWaterMap();
	const bool isAir = militaryManager->GetEnemyCost(CCircuitDef::RoleType::AIR) > 1.f;
	const SFactoryDef::Tiers& tiers = isAir ? facDef.airTiers : isWaterMap ? facDef.waterTiers : facDef.landTiers;
	auto facIt = tiers.begin();
	if ((metalIncome >= facDef.incomes[facIt->first]) && !(facDef.isRequireEnergy && economyManager->IsEnergyEmpty())) {
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

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	static std::vector<std::pair<CCircuitDef*, float>> candidates;  // NOTE: micro-opt
//	candidates.reserve(facDef.buildDefs.size());
	const int frame = circuit->GetLastFrame();
	const float energyNet = economyManager->GetAvgEnergyIncome() - economyManager->GetEnergyUse();
	const float maxCost = militaryManager->GetArmyCost();
	const float range = unit->GetCircuitDef()->GetBuildDistance();
	const AIFloat3& pos = unit->GetPos(frame);
	float magnitude = 0.f;
	for (unsigned i = 0; i < facDef.buildDefs.size(); ++i) {
		CCircuitDef* bd = facDef.buildDefs[i];
		if (((bd->GetCloakCost() > .1f) && (energyNet < bd->GetCloakCost())) ||
			(bd->GetCost() > maxCost) ||
			!bd->IsAvailable(frame) ||
			!terrainManager->CanBeBuiltAt(bd, pos, range))
		{
			continue;
		}

		// (probs[i] + response_weight) hints preferable buildDef within same role
		float prob = militaryManager->RoleProbability(bd) * (probs[i] + reWeight);
		if (prob > 0.f) {
			candidates.push_back(std::make_pair(bd, prob));
			magnitude += prob;
		}
	}

	bool isResponse = !candidates.empty();
	if (!isResponse) {
		// When isResponse==false: candidates.empty() and magnitude==0
		for (unsigned i = 0; i < facDef.buildDefs.size(); ++i) {
			CCircuitDef* bd = facDef.buildDefs[i];
			if (((bd->GetCloakCost() > .1f) && (energyNet < bd->GetCloakCost())) ||
				!bd->IsAvailable(frame) ||
				!terrainManager->CanBeBuiltAtSafe(bd, pos, range))
			{
				continue;
			}
			candidates.push_back(std::make_pair(bd, probs[i]));
			magnitude += probs[i];
		}
	}

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

	if ((buildDef != nullptr) && buildDef->IsAvailable(frame)) {
		UnitDef* def = unit->GetCircuitDef()->GetUnitDef();
		float radius = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE * 4;
		// FIXME CCircuitDef::RoleType <-> CRecruitTask::RecruitType relations
		return EnqueueTask(isResponse ? CRecruitTask::Priority::HIGH : CRecruitTask::Priority::NORMAL,
						   buildDef, pos, CRecruitTask::RecruitType::FIREPOWER, radius);
	}
	return nullptr;
}

bool CFactoryManager::IsHighPriority(CAllyUnit* unit) const
{
	auto it = unfinishedUnits.find(unit);
	if (it == unfinishedUnits.end()) {
		return false;
	}
	return IBuilderTask::Priority::HIGH == it->second->GetPriority();
}

CCircuitDef* CFactoryManager::GetFactoryToBuild(AIFloat3 position, bool isStart)
{
	CCircuitDef* facDef = factoryData->GetFactoryToBuild(circuit, position, isStart);
	if ((facDef == nullptr) && utils::is_valid(position)) {
		facDef = factoryData->GetFactoryToBuild(circuit, -RgtVector, isStart);
	}
	return facDef;
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

	airpadDef = circuit->GetCircuitDef(root["economy"].get("airpad", "").asCString());
	if (airpadDef == nullptr) {
		airpadDef = circuit->GetEconomyManager()->GetDefaultDef();
	}

	/*
	 * Roles, attributes and retreat
	 */
	std::map<CCircuitDef::RoleType, std::set<CCircuitDef::Id>> roleDefs;
	CCircuitDef::RoleName& roleNames = CCircuitDef::GetRoleNames();
	CCircuitDef::AttrName& attrNames = CCircuitDef::GetAttrNames();
	CCircuitDef::FireName& fireNames = CCircuitDef::GetFireNames();
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
		cdef->SetMainRole(it->second);
		cdef->AddRole(it->second);
		roleDefs[it->second].insert(cdef->GetId());

//		if (role.size() < 2) {
			cdef->AddEnemyRole(it->second);
//		} else {
			for (unsigned i = 1; i < role.size(); ++i) {
				const std::string& enemyName = role[i].asString();
				it = roleNames.find(enemyName);
				if (it == roleNames.end()) {
					circuit->LOG("CONFIG %s: %s has unknown enemy role '%s'", cfgName.c_str(), defName.c_str(), enemyName.c_str());
					continue;
				}
				cdef->AddEnemyRole(it->second);
//				cdef->AddRole(it->second);
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
					cdef->AddAttribute(it->second);
				}
			} else {
				cdef->AddRole(it->second);
				roleDefs[it->second].insert(cdef->GetId());
			}
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

		cdef->SetRetreat(behaviour.get("retreat", cdef->GetRetreat()).asFloat());

		const Json::Value& pwrMod = behaviour["pwr_mod"];
		if (!pwrMod.isNull()) {
			cdef->ModPower(pwrMod.asFloat());
		}
		const Json::Value& thrMod = behaviour["thr_mod"];
		if (!thrMod.isNull()) {
			cdef->ModThreat(thrMod.asFloat());
		}
	}

	/*
	 * Factories
	 */
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	const Json::Value& factories = root["factory"];
	for (const std::string& fac : factories.getMemberNames()) {
		CCircuitDef* cdef = circuit->GetCircuitDef(fac.c_str());
		if (cdef == nullptr) {
			circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), fac.c_str());
			continue;
		}

		const Json::Value& factory = factories[fac];
		SFactoryDef facDef;

		// NOTE: used to create tasks on Event (like DefendTask), fix/improve
		const std::unordered_set<CCircuitDef::Id>& options = cdef->GetBuildOptions();
		facDef.roleDefs.resize(static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_), nullptr);
		for (CCircuitDef::RoleT i = 0; i < static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_); ++i) {
			float minCost = std::numeric_limits<float>::max();
			CCircuitDef* rdef = nullptr;
			const CCircuitDef::RoleType type = static_cast<CCircuitDef::RoleType>(i);
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

		const Json::Value& items = factory["unit"];
		const Json::Value& tiers = factory["income_tier"];
		facDef.buildDefs.reserve(items.size());
		const unsigned tierSize = tiers.size();
		facDef.incomes.reserve(tierSize + 1);

		CCircuitDef* landDef = nullptr;
		CCircuitDef* waterDef = nullptr;
		float landSize = std::numeric_limits<float>::max();
		float waterSize = std::numeric_limits<float>::max();

		for (unsigned i = 0; i < items.size(); ++i) {
			CCircuitDef* udef = circuit->GetCircuitDef(items[i].asCString());
			if (udef == nullptr) {
				circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), items[i].asCString());
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
			facDef.airTiers = terrainManager->IsWaterMap() ? facDef.waterTiers : facDef.landTiers;
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
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::UpdateIdle, this), interval, offset + 0);
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::UpdateFactory, this), interval, offset + 2);
	};

	circuit->GetSetupManager()->ExecOnFindStart(subinit);
}

void CFactoryManager::Release()
{
	// NOTE: Release expected to be called on CCircuit::Release.
	//       It doesn't stop scheduled GameTasks for that reason.
	for (IUnitTask* task : updateTasks) {
		AbortTask(task);
		// NOTE: Do not delete task as other AbortTask may ask for it
	}
	updateTasks.clear();
}

void CFactoryManager::EnableFactory(CCircuitUnit* unit)
{
	factoryPower += unit->GetBuildSpeed();

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
			// NOTE: OOAICallback::GetFriendlyUnits may return yet unregistered units created in GamePreload
			if (ass != nullptr) {
				nanos.insert(ass);

				std::set<CCircuitUnit*>& facs = assists[ass];
				if (facs.empty()) {
					factoryPower += ass->GetBuildSpeed();
				}
				facs.insert(unit);
			}
		}
		delete ndef;
		delete nano;
	}
//	utils::free_clear(units);

	if (factories.empty()) {
		this->circuit->GetSetupManager()->SetBasePos(pos);
		this->circuit->GetMilitaryManager()->MakeBaseDefence(pos);
	}

	auto it = factoryDefs.find(unit->GetCircuitDef()->GetId());
	if (it != factoryDefs.end()) {
		const SFactoryDef& facDef = it->second;
		factories.emplace_back(unit, nanos, facDef.nanoCount, facDef.GetRoleDef(CCircuitDef::RoleType::BUILDER));
	} else {
		factories.emplace_back(unit, nanos, 0, nullptr);
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
		CBuilderManager* builderManager = this->circuit->GetBuilderManager();
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
			std::set<IBuilderTask*> tasks = builderManager->GetTasks(IBuilderTask::BuildType::FACTORY);
			for (IBuilderTask* task : tasks) {
				auto it = factoryDefs.find(task->GetBuildDef()->GetId());
				if (it != factoryDefs.end()) {
					const SFactoryDef& facDef = it->second;
					CCircuitDef* bdef = facDef.GetRoleDef(CCircuitDef::RoleType::BUILDER);
					hasBuilder = ((bdef != nullptr) && bdef->IsAvailable(frame));
					if (hasBuilder) {
						break;
					} else if (task->GetTarget() == nullptr) {
						builderManager->AbortTask(task);
					}
				}
			}
			if (!hasBuilder) {
				// queue new factory with builder
				CCircuitDef* facDef = GetFactoryToBuild(-RgtVector, true);
				if (facDef != nullptr) {
					builderManager->EnqueueFactory(IBuilderTask::Priority::NOW, facDef, -RgtVector);
				}
			}
		}
	};

	if (unit->GetTask()->GetType() == IUnitTask::Type::NIL) {
		checkBuilderFactory(circuit->GetLastFrame());
		return;
	}
	factoryPower -= unit->GetBuildSpeed();
	for (auto it = factories.begin(); it != factories.end(); ++it) {
		if (it->unit != unit) {
			continue;
		}
		for (CCircuitUnit* ass : it->nanos) {
			std::set<CCircuitUnit*>& facs = assists[ass];
			facs.erase(unit);
			if (facs.empty()) {
				factoryPower -= ass->GetBuildSpeed();
			}
		}
//			factories.erase(it);  // NOTE: micro-opt
		*it = factories.back();
		factories.pop_back();
		break;
	}

	if (!factories.empty()) {
		const AIFloat3& pos = factories.front().unit->GetPos(circuit->GetLastFrame());
		circuit->GetSetupManager()->SetBasePos(pos);
	}

	if (unit->GetCircuitDef()->GetMobileId() < 0) {
		validAir.erase(unit);
	}

	checkBuilderFactory(circuit->GetLastFrame());
}

IUnitTask* CFactoryManager::CreateFactoryTask(CCircuitUnit* unit)
{
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	const bool isStalling = economyManager->IsMetalEmpty() &&
							(economyManager->GetAvgMetalIncome() * 1.2f < economyManager->GetMetalPull()) &&
							(metalPull * economyManager->GetPullMtoS() > circuit->GetBuilderManager()->GetMetalPull());
	const bool isNotReady = !economyManager->IsExcessed() || isStalling;
	if (isNotReady) {
		return EnqueueWait(false, FRAMES_PER_SEC * 3);
	}

	if (unit->GetCircuitDef()->GetMobileId() < 0) {
		if (circuit->GetMilitaryManager()->IsAirValid()) {
			if (validAir.find(unit) == validAir.end()) {
				EnableFactory(unit);
			}
		} else {
			if (validAir.find(unit) != validAir.end()) {
				DisableFactory(unit);
			}
			return EnqueueWait(false, FRAMES_PER_SEC * 10);
		}
	}

	IUnitTask* task = UpdateBuildPower(unit);
	if (task != nullptr) {
		return task;
	}

	task = UpdateFirePower(unit);
	if (task != nullptr) {
		return task;
	}

	return EnqueueWait(false, FRAMES_PER_SEC * 3);
}

IUnitTask* CFactoryManager::CreateAssistTask(CCircuitUnit* unit)
{
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	bool isMetalEmpty = economyManager->IsMetalEmpty();
	CAllyUnit* repairTarget = nullptr;
	CAllyUnit* buildTarget = nullptr;
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	float radius = unit->GetCircuitDef()->GetBuildDistance();

	CBuilderManager* builderManager = circuit->GetBuilderManager();
	CCircuitDef* terraDef = builderManager->GetTerraDef();
	const float maxCost = MAX_BUILD_SEC * economyManager->GetAvgMetalIncome() * economyManager->GetEcoFactor();
	float curCost = std::numeric_limits<float>::max();
	circuit->UpdateFriendlyUnits();
	// NOTE: OOAICallback::GetFriendlyUnitsIn depends on unit's radius
	auto units = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius * 0.9f));
	for (Unit* u : units) {
		CAllyUnit* candUnit = circuit->GetFriendlyUnit(u);
		if ((candUnit == nullptr) || builderManager->IsReclaimed(candUnit)) {
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
	utils::free_clear(units);
	if (/*!isMetalEmpty && */(buildTarget != nullptr)) {
		// Construction task
		IBuilderTask::Priority priority = buildTarget->GetCircuitDef()->IsMobile() ?
										  IBuilderTask::Priority::HIGH :
										  IBuilderTask::Priority::NORMAL;
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

	return EnqueueWait(false, FRAMES_PER_SEC * 3);
}

void CFactoryManager::Watchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	auto checkIdler = [this](CCircuitUnit* unit) {
		if (unit->GetTask()->GetType() == IUnitTask::Type::PLAYER) {
			return;
		}
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
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	idleTask->Update();
}

void CFactoryManager::UpdateFactory()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	if (updateIterator >= updateTasks.size()) {
		updateIterator = 0;
	}

	int lastFrame = circuit->GetLastFrame();
	// stagger the Update's
	unsigned int n = (updateTasks.size() / TEAM_SLOWUPDATE_RATE) + 1;

	while ((updateIterator < updateTasks.size()) && (n != 0)) {
		IUnitTask* task = updateTasks[updateIterator];
		if (task->IsDead()) {
			updateTasks[updateIterator] = updateTasks.back();
			updateTasks.pop_back();
			delete task;
		} else {
			int frame = task->GetLastTouched();
			int timeout = task->GetTimeout();
			if ((frame != -1) && (timeout > 0) && (lastFrame - frame >= timeout)) {
				AbortTask(task);
			} else {
				task->Update();
			}
			++updateIterator;
			n--;
		}
	}
}

} // namespace circuit
