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

//		unit->GetUnit()->SetFireState(2);

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
		bool hasBuilder = (it != factoryDefs.end()) && (it->second.builderDef != nullptr);
		factories.emplace_back(unit, nanos, 1, hasBuilder);

//		this->circuit->GetSetupManager()->SetBasePos(pos);
	};
	auto factoryIdleHandler = [this](CCircuitUnit* unit) {
		unit->GetTask()->OnUnitIdle(unit);
	};
	auto factoryDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

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
				CCircuitDef* facDef = GetFactoryToBuild(this->circuit);
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
			factories.erase(it);
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

		int frame = this->circuit->GetLastFrame();
		const AIFloat3& assPos = unit->GetPos(frame);
		unit->GetUnit()->ExecuteCustomCommand(CMD_PRIORITY, {0.0f});

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
					it = havens.erase(it);
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
			} else {
				createdHandler[unitDefId] = assistCreatedHandler;
				finishedHandler[unitDefId] = assistFinishedHandler;
				idleHandler[unitDefId] = assistIdleHandler;
				destroyedHandler[unitDefId] = assistDestroyedHandler;
				assistDef = cdef;
			}
		}
	}

	factoryData = circuit->GetAllyTeam()->GetFactoryData().get();
	ReadConfig();

	// FIXME: EXPERIMENTAL
	/*
	 * striderhub handlers
	 */
	CCircuitDef::Id defId = circuit->GetCircuitDef("striderhub")->GetId();
	finishedHandler[defId] = [this, defId](CCircuitUnit* unit) {
		unit->SetManager(this);

		factoryPower += unit->GetCircuitDef()->GetBuildSpeed();
		CRecruitTask* task = new CRecruitTask(this, IUnitTask::Priority::HIGH, nullptr, ZeroVector,
											  CRecruitTask::RecruitType::FIREPOWER, unit->GetCircuitDef()->GetBuildDistance());
		unit->SetTask(task);

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
		factories.emplace_back(unit, nanos, 4, false);

		unit->GetUnit()->ExecuteCustomCommand(CMD_PRIORITY, {2.0f});
//		unit->GetUnit()->SetRepeat(true);

		idleHandler[defId](unit);
	};
	idleHandler[defId] = [this](CCircuitUnit* unit) {
		CTerrainManager* terrainManager = this->circuit->GetTerrainManager();
		int frame = this->circuit->GetLastFrame();
		bool isWaterMap = terrainManager->GetPercentLand() < 40.0;

		CEconomyManager* mgr = this->circuit->GetEconomyManager();
		const float metalIncome = std::min(mgr->GetAvgMetalIncome(), mgr->GetAvgEnergyIncome()) * mgr->GetEcoFactor();
		std::map<unsigned, std::vector<float>>& tiers = isWaterMap ? striderHubDef.waterTiers : striderHubDef.landTiers;

		auto facIt = tiers.begin();
		if ((metalIncome >= striderHubDef.incomes[facIt->first]) && !(striderHubDef.isRequireEnergy && mgr->IsEnergyEmpty())) {
			while (facIt != tiers.end()) {
				if (metalIncome < striderHubDef.incomes[facIt->first]) {
					break;
				}
				++facIt;
			}
			if (facIt == tiers.end()) {
				--facIt;
			}
		}
		const std::vector<float>& probs = facIt->second;

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

		CCircuitDef* striderDef = striderHubDef.buildDefs[choice];
		if (striderDef != nullptr) {
			AIFloat3 pos = unit->GetPos(frame);
			const float size = DEFAULT_SLACK / 2;
			switch (unit->GetUnit()->GetBuildingFacing()) {
				default:
				case UNIT_FACING_SOUTH: {  // z++
					pos.z += size;
				} break;
				case UNIT_FACING_EAST: {  // x++
					pos.x += size;
				} break;
				case UNIT_FACING_NORTH: {  // z--
					pos.z -= size;
				} break;
				case UNIT_FACING_WEST: {  // x--
					pos.x -= size;
				} break;
			}
			pos = terrainManager->FindBuildSite(striderDef, pos, this->circuit->GetCircuitDef("striderhub")->GetBuildDistance(), -1);
			if (pos != -RgtVector) {
				unit->GetUnit()->Build(striderDef->GetUnitDef(), pos, -1, 0, frame + FRAMES_PER_SEC * 10);
			}
		}
	};
	destroyedHandler[defId] = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		if (unit->GetUnit()->IsBeingBuilt()) {
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
			factories.erase(it);
			break;
		}
		delete unit->GetTask();
	};
	// FIXME: EXPERIMENTAL
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
	// TODO: Convert CRecruitTask into IBuilderTask
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

IUnitTask* CFactoryManager::GetTask(CCircuitUnit* unit)
{
	IUnitTask* task = nullptr;

	if (unit->GetCircuitDef() == assistDef) {  // FIXME: Check Id instead pointers?
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
	std::list<CCircuitUnit*> facs;
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
	return *it;
}

//CCircuitUnit* CFactoryManager::GetRandomFactory(const AIFloat3& position, CCircuitDef::RoleType role)
//{
//	CTerrainManager* terrainManager = circuit->GetTerrainManager();
//	AIFloat3 pos = position;
//	terrainManager->CorrectPosition(pos);
//	int iS = terrainManager->GetSectorIndex(pos);
//	std::list<CCircuitUnit*> facs;
//	for (SFactory& fac : factories) {
//		STerrainMapArea* area = fac.unit->GetArea();
//		if ((area == nullptr) || (area->sector.find(iS) == area->sector.end())) {
//			continue;
//		}
//		for (CCircuitDef::Id bdId : fac.unit->GetCircuitDef()->GetBuildOptions()) {
//			if (circuit->GetCircuitDef(bdId)->IsRoleRiot()) {
//				facs.push_back(fac.unit);
//				break;
//			}
//		}
//	}
//	if (facs.empty()) {
//		return nullptr;
//	}
//	auto it = facs.begin();
//	std::advance(it, rand() % facs.size());
//	return *it;
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

CRecruitTask* CFactoryManager::UpdateBuildPower(CCircuitUnit* unit)
{
	if (!CanEnqueueTask()) {
		return nullptr;
	}

	CEconomyManager* economyManager = circuit->GetEconomyManager();
	float metalIncome = std::min(economyManager->GetAvgMetalIncome(), economyManager->GetAvgEnergyIncome());
	if ((circuit->GetBuilderManager()->GetBuilderPower() >= metalIncome * 1.5f) || (rand() >= RAND_MAX / 2)) {
		return nullptr;
	}

	auto it = factoryDefs.find(unit->GetCircuitDef()->GetId());
	if (it == factoryDefs.end()) {
		return nullptr;
	}
	CCircuitDef* buildDef = it->second.builderDef;
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
	CCircuitDef* buildDef;

	auto it = factoryDefs.find(unit->GetCircuitDef()->GetId());
	if (it == factoryDefs.end()) {
		return nullptr;
	}
	const SFactoryDef& facDef = it->second;

	buildDef = facDef.antiAirDef;
	if ((buildDef != nullptr) && circuit->GetMilitaryManager()->IsNeedAA(buildDef) && buildDef->IsAvailable()) {
		const AIFloat3& buildPos = unit->GetPos(circuit->GetLastFrame());
		UnitDef* def = unit->GetCircuitDef()->GetUnitDef();
		float radius = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE * 4;
		return EnqueueTask(CRecruitTask::Priority::NORMAL, buildDef, buildPos, CRecruitTask::RecruitType::AA, radius);
	}
	buildDef = facDef.artyDef;
	if ((buildDef != nullptr) && circuit->GetMilitaryManager()->IsNeedArty(buildDef) && buildDef->IsAvailable()) {
		const AIFloat3& buildPos = unit->GetPos(circuit->GetLastFrame());
		UnitDef* def = unit->GetCircuitDef()->GetUnitDef();
		float radius = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE * 4;
		return EnqueueTask(CRecruitTask::Priority::NORMAL, buildDef, buildPos, CRecruitTask::RecruitType::ARTY, radius);
	}

	CEconomyManager* mgr = circuit->GetEconomyManager();
	const float metalIncome = std::min(mgr->GetAvgMetalIncome(), mgr->GetAvgEnergyIncome()) * mgr->GetEcoFactor();
	auto facIt = facDef.tiers.begin();
	if ((metalIncome >= facDef.incomes[facIt->first]) && !(facDef.isRequireEnergy && mgr->IsEnergyEmpty())) {
		while (facIt != facDef.tiers.end()) {
			if (metalIncome < facDef.incomes[facIt->first]) {
				break;
			}
			++facIt;
		}
		if (facIt == facDef.tiers.end()) {
			--facIt;
		}
	}
	const std::vector<float>& probs = facIt->second;

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
	if ((buildDef != nullptr) && buildDef->IsAvailable()) {
		const AIFloat3& buildPos = unit->GetPos(circuit->GetLastFrame());
		UnitDef* def = unit->GetCircuitDef()->GetUnitDef();
		float radius = std::max(def->GetXSize(), def->GetZSize()) * SQUARE_SIZE * 4;
		return EnqueueTask(CRecruitTask::Priority::NORMAL, buildDef, buildPos, CRecruitTask::RecruitType::FIREPOWER, radius);
	}

	return nullptr;
}

CCircuitDef* CFactoryManager::GetFactoryToBuild(CCircuitAI* circuit, bool isStart)
{
	return factoryData->GetFactoryToBuild(circuit, isStart);
}

float CFactoryManager::GetStriderChance() const
{
	return factoryData->GetStriderChance();
}

void CFactoryManager::AddFactory(CCircuitDef* cdef)
{
	factoryData->AddFactory(cdef);
}

void CFactoryManager::DelFactory(CCircuitDef* cdef)
{
	factoryData->DelFactory(cdef);
}

CCircuitDef* CFactoryManager::GetBuilderDef(CCircuitDef* facDef) const
{
	auto it = factoryDefs.find(facDef->GetId());
	return (it != factoryDefs.end()) ? it->second.builderDef : nullptr;
}

void CFactoryManager::ReadConfig()
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();

	/*
	 * Factories
	 */
	const Json::Value& factories = root["factories"];
	for (const std::string& fac : factories.getMemberNames()) {
		CCircuitDef* cdef = circuit->GetCircuitDef(fac.c_str());
		if (cdef == nullptr) {
			continue;
		}
		const Json::Value& factory = factories[fac];
		SFactoryDef facDef;

		facDef.builderDef = circuit->GetCircuitDef(factory.get("builder_def", "").asCString());
		if (facDef.builderDef != nullptr) {
			facDef.builderDef->SetRole(CCircuitDef::RoleType::BUILDER);
		}
		facDef.antiAirDef = circuit->GetCircuitDef(factory.get("anti_air_def", "").asCString());
		if (facDef.antiAirDef != nullptr) {
			facDef.antiAirDef->SetRole(CCircuitDef::RoleType::AA);
		}
		facDef.artyDef = circuit->GetCircuitDef(factory.get("artillery_def", "").asCString());
		if (facDef.artyDef != nullptr) {
			facDef.artyDef->SetRole(CCircuitDef::RoleType::ARTY);
		}

		facDef.isRequireEnergy = factory.get("require_energy", false).asBool();

		const Json::Value& items = factory["unit_def"];
		const Json::Value& tiers = factory["income_tier"];
		facDef.buildDefs.reserve(items.size());
		const unsigned tierSize = tiers.size();
		facDef.incomes.reserve(tierSize);
		for (unsigned i = 0; i < items.size(); ++i) {
			CCircuitDef* udef = circuit->GetCircuitDef(items[i].asCString());
			if (udef == nullptr) {
				continue;
			}
			facDef.buildDefs.push_back(udef);
		}

		if (facDef.buildDefs.empty()) {
			facDef.buildDefs.push_back(nullptr);
		} else {
			const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();
			auto fillProbs = [this, &cfgName, &facDef, &fac, &factory](unsigned i) {
				const Json::Value& tier = factory[utils::int_to_string(i, "tier%i")];
				if (tier == Json::Value::null) {
					return false;
				}
				std::vector<float>& probs = facDef.tiers[i];
				probs.reserve(facDef.buildDefs.size());
				float sum = .0f;
				for (unsigned j = 0; j < facDef.buildDefs.size(); ++j) {
					const float p = tier[j].asFloat();
					sum += p;
					probs.push_back(p);
				}
				if (fabs(sum - 1.0f) > 0.0001f) {
					circuit->LOG("CONFIG %s: %s's tier%i total probability = %f", cfgName.c_str(), fac.c_str(), i, sum);
				}
				return true;
			};
			unsigned i = 0;
			for (; i < tierSize; ++i) {
				facDef.incomes.push_back(tiers[i].asFloat());
				fillProbs(i);
			}
			fillProbs(i);
		}
		if (facDef.incomes.empty()) {
			facDef.incomes.push_back(std::numeric_limits<float>::max());
		}
		if (facDef.tiers.empty()) {
			facDef.tiers[0];  // create empty tier
		}

		factoryDefs[cdef->GetId()] = facDef;
	}

	/*
	 * Strider hub
	 */
	const Json::Value& striderHub = root["strider"];
	const Json::Value& items = striderHub["unit_def"];
	const Json::Value& tiers = striderHub["income_tier"];

	striderHubDef.isRequireEnergy = striderHub.get("require_energy", false).asBool();

	striderHubDef.buildDefs.reserve(items.size());
	const unsigned tierSize = tiers.size();
	striderHubDef.incomes.reserve(tierSize);
	for (unsigned i = 0; i < items.size(); ++i) {
		CCircuitDef* udef = circuit->GetCircuitDef(items[i].asCString());
		if (udef == nullptr) {
			continue;
		}
		striderHubDef.buildDefs.push_back(udef);
	}

	if (striderHubDef.buildDefs.empty()) {
		striderHubDef.buildDefs.push_back(nullptr);
	} else {
		const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();
		auto fillProbs = [this, &cfgName, &striderHub](unsigned i, const char* tierType, std::vector<float>& probs) {
			const Json::Value& tier = striderHub[utils::int_to_string(i, std::string(tierType) + "%i")];
			if (tier == Json::Value::null) {
				return false;
			}
			probs.reserve(striderHubDef.buildDefs.size());
			float sum = .0f;
			for (unsigned j = 0; j < striderHubDef.buildDefs.size(); ++j) {
				const float p = tier[j].asFloat();
				sum += p;
				probs.push_back(p);
			}
			if (fabs(sum - 1.0f) > 0.0001f) {
				circuit->LOG("CONFIG %s: strider's %s%i total probability = %f", cfgName.c_str(), tierType, i, sum);
			}
			return true;
		};
		unsigned i = 0;
		for (; i < tierSize; ++i) {
			striderHubDef.incomes.push_back(tiers[i].asFloat());
			fillProbs(i, "land_tier", striderHubDef.landTiers[i]);
			fillProbs(i, "water_tier", striderHubDef.waterTiers[i]);
		}
		fillProbs(i, "land_tier", striderHubDef.landTiers[i]);
		fillProbs(i, "water_tier", striderHubDef.waterTiers[i]);
	}
	if (striderHubDef.incomes.empty()) {
		striderHubDef.incomes.push_back(std::numeric_limits<float>::max());
	}
	if (striderHubDef.landTiers.empty()) {
		striderHubDef.landTiers[0];  // create empty tier
	}
	if (striderHubDef.waterTiers.empty()) {
		striderHubDef.waterTiers[0];  // create empty tier
	}

	std::vector<std::pair<const char*, CCircuitDef::RoleType>> roles = {
		std::make_pair("scout",     CCircuitDef::RoleType::SCOUT),
		std::make_pair("bomber",    CCircuitDef::RoleType::BOMBER),
		std::make_pair("riot",      CCircuitDef::RoleType::RIOT),
		std::make_pair("melee",     CCircuitDef::RoleType::MELEE),
		std::make_pair("artillery", CCircuitDef::RoleType::ARTY),
	};
	for (auto& pair : roles) {
		for (const Json::Value& scout : root[pair.first]) {
			CCircuitDef* cdef = circuit->GetCircuitDef(scout.asCString());
			if (cdef == nullptr) {
				continue;
			}
			cdef->SetRole(pair.second);
		}
	}
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
		for (auto task : deleteAssists) {
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
