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
#include "task/static/WaitTask.h"
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
	const int interval = 4;
	const int offset = circuit->GetSkirmishAIId() % interval;
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::UpdateIdle, this), interval, offset + 0);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CFactoryManager::UpdateFactory, this), interval, offset + 2);

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

		if (factories.empty()) {
			this->circuit->GetSetupManager()->SetBasePos(pos);
			BaseDefence(pos);
		}

		auto it = factoryDefs.find(unit->GetCircuitDef()->GetId());
		if (it != factoryDefs.end()) {
			const SFactoryDef& facDef = it->second;
			bool hasBuilder = (facDef.GetRoleDef(CCircuitDef::RoleType::BUILDER) != nullptr);
			factories.emplace_back(unit, nanos, facDef.nanoCount, hasBuilder);
		} else {
			factories.emplace_back(unit, nanos, 0, false);
		}
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
			// check if any factory with builders left
			bool hasBuilder = false;
			for (SFactory& fac : factories) {
				if (fac.hasBuilder) {
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
						hasBuilder = (facDef.GetRoleDef(CCircuitDef::RoleType::BUILDER) != nullptr);
						if (hasBuilder) {
							break;
						}
						builderManager->AbortTask(task);
					}
				}
				if (!hasBuilder) {
					// queue new factory with builder
					CCircuitDef* facDef = GetFactoryToBuild(-RgtVector, true);
					if (facDef != nullptr) {
						builderManager->EnqueueTask(IBuilderTask::Priority::NOW, facDef, -RgtVector,
													IBuilderTask::BuildType::FACTORY);
					}
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

		if (!factories.empty()) {
			const AIFloat3& pos = factories.front().unit->GetPos(this->circuit->GetLastFrame());
			this->circuit->GetSetupManager()->SetBasePos(pos);
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
	CCircuitDef* commDef = circuit->GetSetupManager()->GetCommChoice();

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
				if (commDef->CanBuild(cdef)) {
					assistCandy = cdef;
				}
			}
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
		auto customParams = std::move(cdef->GetUnitDef()->GetCustomParams());
		if (customParams.find("level") != customParams.end()) {
			cdef->SetEnemyRole(CCircuitDef::RoleType::HEAVY);
			cdef->AddRole(CCircuitDef::RoleType::HEAVY);
		}
	}

	assistDef = assistCandy;

	factoryData = circuit->GetAllyTeam()->GetFactoryData().get();
	ReadConfig();
}

CFactoryManager::~CFactoryManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(updateTasks);
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
	factoryTasks.push_back(task);
	updateTasks.push_back(task);
	return task;
}

IUnitTask* CFactoryManager::EnqueueWait(int timeout)
{
	CWaitTask* task = new CWaitTask(this, timeout);
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
											 CCircuitUnit* target)
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
		default: break;  // RECLAIM, WAIT
	}
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

CCircuitUnit* CFactoryManager::GetClosestFactory(AIFloat3 position)
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	terrainManager->CorrectPosition(position);
	int iS = terrainManager->GetSectorIndex(position);
	CCircuitUnit* factory = nullptr;
	float minSqDist = std::numeric_limits<float>::max();
	int frame = circuit->GetLastFrame();
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
		if ((area != nullptr) && (area->sector.find(iS) == area->sector.end())) {
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
	const float metalIncome = std::min(economyManager->GetAvgMetalIncome(), economyManager->GetAvgEnergyIncome());
	if ((circuit->GetBuilderManager()->GetBuildPower() >= metalIncome * bpRatio) || (rand() >= RAND_MAX / 2)) {
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
	const bool isWaterMap = circuit->GetTerrainManager()->IsWaterMap();
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
	const float energyNet = em->GetAvgEnergyIncome() - em->GetEnergyPull();
	const float maxCost = militaryManager->GetArmyCost();
	float magnitude = 0.f;
	for (unsigned i = 0; i < facDef.buildDefs.size(); ++i) {
		CCircuitDef* bd = facDef.buildDefs[i];
		if (((bd->GetCloakCost() > .1f) && (energyNet < bd->GetCloakCost())) ||
			(bd->GetCost() > maxCost))
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
		// NOTE: Ignores cloakCost and maxCost
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

	/*
	 * Roles, attributes and retreat
	 */
	std::map<CCircuitDef::RoleType, std::set<CCircuitDef::Id>> roleDefs;
	CCircuitDef::RoleName& roleNames = CCircuitDef::GetRoleNames();
	CCircuitDef::AttrName& attrNames = CCircuitDef::GetAttrNames();
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

		const Json::Value& enemyRole = role[1];
		if (enemyRole.isNull()) {
			cdef->SetEnemyRole(it->second);
		} else {
			const std::string& enemyName = enemyRole.asString();
			it = roleNames.find(enemyName);
			if (it == roleNames.end()) {
				circuit->LOG("CONFIG %s: %s has unknown enemy role '%s'", cfgName.c_str(), defName.c_str(), enemyName.c_str());
				continue;
			}
			cdef->SetEnemyRole(it->second);
			cdef->AddRole(it->second);
		}

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

		const Json::Value& limit = behaviour["limit"];
		if (!limit.isNull()) {
			cdef->SetMaxThisUnit(std::min(limit.asInt(), cdef->GetMaxThisUnit()));
		}

		cdef->SetRetreat(behaviour.get("retreat", cdef->GetRetreat()).asFloat());
	}

	/*
	 * Factories
	 */
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	const Json::Value& factories = root["factory"]["unit"];
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
		facDef.incomes.reserve(tierSize);

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

	bpRatio = root["economy"].get("buildpower", 1.f).asFloat();
	reWeight = root["response"].get("_weight_", .5f).asFloat();
}

void CFactoryManager::BaseDefence(const AIFloat3& pos)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	for (auto& kv : circuit->GetMilitaryManager()->GetBaseDefence()) {
		CCircuitDef* buildDef = kv.first;
		if (buildDef->IsAvailable()) {
			scheduler->RunTaskAt(std::make_shared<CGameTask>([this, buildDef, pos]() {
				circuit->GetBuilderManager()->EnqueueTask(IBuilderTask::Priority::NORMAL, buildDef, pos,
														  IBuilderTask::BuildType::DEFENCE, 0.f, true, 0);
			}), FRAMES_PER_SEC * kv.second);
		}
	}
}

IUnitTask* CFactoryManager::CreateFactoryTask(CCircuitUnit* unit)
{
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	if (economyManager->GetAvgMetalIncome() * 2.0f < economyManager->GetMetalPull()) {
		return EnqueueWait(FRAMES_PER_SEC * 5);
	}

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

IUnitTask* CFactoryManager::CreateAssistTask(CCircuitUnit* unit)
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

	return EnqueueWait(FRAMES_PER_SEC * 3);
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

void CFactoryManager::UpdateFactory()
{
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
