/*
 * BuilderManager.cpp
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "module/MilitaryManager.h"
#include "module/FactoryManager.h"
#include "resource/MetalManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "terrain/PathFinder.h"
#include "task/NilTask.h"
#include "task/IdleTask.h"
#include "task/RetreatTask.h"
#include "task/builder/WaitTask.h"
#include "task/builder/FactoryTask.h"
#include "task/builder/NanoTask.h"
#include "task/builder/StoreTask.h"
#include "task/builder/PylonTask.h"
#include "task/builder/EnergyTask.h"
#include "task/builder/DefenceTask.h"
#include "task/builder/BunkerTask.h"
#include "task/builder/BigGunTask.h"
#include "task/builder/RadarTask.h"
#include "task/builder/SonarTask.h"
#include "task/builder/MexTask.h"
#include "task/builder/TerraformTask.h"
#include "task/builder/RepairTask.h"
#include "task/builder/ReclaimTask.h"
#include "task/builder/PatrolTask.h"
#include "task/builder/GuardTask.h"
#include "task/builder/BuildChain.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#include "json/json.h"

#include "Command.h"
#include "Log.h"

namespace circuit {

using namespace springai;

CBuilderManager::CBuilderManager(CCircuitAI* circuit)
		: IUnitModule(circuit)
		, buildTasksCount(0)
		, buildPower(.0f)
		, buildIterator(0)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::Watchdog, this),
							FRAMES_PER_SEC * 60,
							circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 10);
	scheduler->RunTaskAt(std::make_shared<CGameTask>(&CBuilderManager::Init, this));

	/*
	 * worker handlers
	 */
	auto workerCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nilTask->AssignTo(unit);
			this->circuit->AddActionUnit(unit);
		}
	};
	auto workerFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			this->circuit->AddActionUnit(unit);
		}
		idleTask->AssignTo(unit);

		++buildAreas[unit->GetArea()][unit->GetCircuitDef()];

		AddBuildPower(unit);
		workers.insert(unit);

		AddBuildList(unit);
	};
	auto workerIdleHandler = [this](CCircuitUnit* unit) {
		// FIXME: Avoid instant task reassignment, its only valid on build order fail.
		//        Can cause idle builder, but Watchdog should catch it eventually.
		//        Unfortunately can't use EVENT_COMMAND_FINISHED because there is no unique commandId like unitId
		if (this->circuit->GetLastFrame() > unit->GetTaskFrame()/* + FRAMES_PER_SEC*/) {
			unit->GetTask()->OnUnitIdle(unit);
		}
	};
	auto workerDamagedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		unit->GetTask()->OnUnitDamaged(unit, attacker);
	};
	auto workerDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		if (task->GetType() == IUnitTask::Type::NIL) {
			return;
		}
		--buildAreas[unit->GetArea()][unit->GetCircuitDef()];

		DelBuildPower(unit);
		workers.erase(unit);

		RemoveBuildList(unit);
	};

	/*
	 * building handlers
	 */
	auto buildingDamagedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		EnqueueRepair(IBuilderTask::Priority::HIGH, unit);
	};
	auto buildingDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		int frame = this->circuit->GetLastFrame();
		int facing = unit->GetUnit()->GetBuildingFacing();
		this->circuit->GetTerrainManager()->DelBlocker(unit->GetCircuitDef(), unit->GetPos(frame), facing);
	};

	/*
	 * heavy handlers
	 */
//	auto heavyCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
//		CEconomyManager* economyManager = this->circuit->GetEconomyManager();
//		if (economyManager->GetAvgMetalIncome() * economyManager->GetEcoFactor() > 32.0f) {
//			TRY_UNIT(this->circuit, unit,
//				unit->GetUnit()->ExecuteCustomCommand(CMD_PRIORITY, {2.0f});
////				EnqueueRepair(IBuilderTask::Priority::LOW, unit);
//			)
//		}
//	};

	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const float builderRet = root["retreat"].get("builder", 0.8f).asFloat();

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	const CCircuitAI::CircuitDefs& allDefs = circuit->GetCircuitDefs();
	for (auto& kv : allDefs) {
		CCircuitDef::Id unitDefId = kv.first;
		CCircuitDef* cdef = kv.second;
		if (cdef->IsMobile()) {
			if (cdef->GetUnitDef()->IsBuilder() && !cdef->GetBuildOptions().empty()) {
				createdHandler[unitDefId]   = workerCreatedHandler;
				finishedHandler[unitDefId]  = workerFinishedHandler;
				idleHandler[unitDefId]      = workerIdleHandler;
				damagedHandler[unitDefId]   = workerDamagedHandler;
				destroyedHandler[unitDefId] = workerDestroyedHandler;

				int mtId = terrainManager->GetMobileTypeId(unitDefId);
				if (mtId >= 0) {  // not air
					workerMobileTypes.insert(mtId);
				}
				workerDefs.insert(cdef);

				if (cdef->GetRetreat() < 0.f) {
					cdef->SetRetreat(builderRet);
				}
//			} else if (cdef->GetCost() > 999.0f) {
//				createdHandler[unitDefId] = heavyCreatedHandler;
			}
		} else {
			damagedHandler[unitDefId]   = buildingDamagedHandler;
			destroyedHandler[unitDefId] = buildingDestroyedHandler;
		}
	}

	/*
	 * cormex handlers; forbid from removing blocker
	 */
	CCircuitDef::Id unitDefId = circuit->GetEconomyManager()->GetMexDef()->GetId();
	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		const AIFloat3& pos = unit->GetPos(this->circuit->GetLastFrame());
		CCircuitDef* mexDef = unit->GetCircuitDef();
		int index = this->circuit->GetMetalManager()->FindNearestSpot(pos);
		if (index < 0) {
			return;
		}
		this->circuit->GetMetalManager()->SetOpenSpot(index, true);
		this->circuit->GetEconomyManager()->SetOpenSpot(index, true);
		if (unit->GetUnit()->IsBeingBuilt()) {
			return;
		}
		// Check mex position in 20 seconds
		this->circuit->GetScheduler()->RunTaskAfter(std::make_shared<CGameTask>([this, mexDef, pos, index]() {
			if (this->circuit->GetEconomyManager()->IsAllyOpenSpot(index) &&
				this->circuit->GetBuilderManager()->IsBuilderInArea(mexDef, pos) &&
				this->circuit->GetTerrainManager()->CanBeBuiltAt(mexDef, pos))  // hostile environment
			{
				EnqueueTask(IBuilderTask::Priority::HIGH, mexDef, pos, IBuilderTask::BuildType::MEX)->SetBuildPos(pos);
				this->circuit->GetEconomyManager()->SetOpenSpot(index, false);
			}
		}), FRAMES_PER_SEC * 20);
	};

	ReadConfig();

	buildTasks.resize(static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::_SIZE_));

	for (auto mtId : workerMobileTypes) {
		for (auto& area : terrainManager->GetMobileTypeById(mtId)->area) {
			buildAreas[&area] = std::map<CCircuitDef*, int>();
		}
	}
	buildAreas[nullptr] = std::map<CCircuitDef*, int>();  // air
}

CBuilderManager::~CBuilderManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(buildUpdates);
	for (auto& kv1 : buildChains) {
		for (auto& kv2 : kv1.second) {
			delete kv2.second;
		}
	}
}

int CBuilderManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetCircuitDef()->GetId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	if (builder == nullptr) {
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		if (!unit->GetCircuitDef()->IsMobile() && !terrainManager->ResignAllyBuilding(unit)) {
			// enemy unit captured
			const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
			terrainManager->AddBlocker(unit->GetCircuitDef(), pos, unit->GetUnit()->GetBuildingFacing());
		}
		return 0; //signaling: OK
	}

	IUnitTask* task = builder->GetTask();
	if ((task == nullptr) || task->GetType() != IUnitTask::Type::BUILDER) {
		return 0; //signaling: OK
	}

	IBuilderTask* taskB = static_cast<IBuilderTask*>(task);
	if (unit->GetUnit()->IsBeingBuilt()) {
		// NOTE: Try to cope with wrong event order, when different units created within same task.
		//       Real example: unit starts building, but hlt kills structure right away. UnitDestroyed invoked and new task assigned to unit.
		//       But for some engine-bugged reason unit is not idle and retries same building. UnitCreated invoked for new task with wrong target.
		//       Next workaround unfortunately doesn't mark bugged building on blocking map.
		// TODO: Create additional task to build/reclaim lost unit
		if ((taskB->GetTarget() == nullptr) && (taskB->GetBuildDef() != nullptr) &&
			(*taskB->GetBuildDef() == *unit->GetCircuitDef()) && taskB->IsEqualBuildPos(unit))
		{
			taskB->UpdateTarget(unit);
			unfinishedUnits[unit] = taskB;
		}
	} else {
		DoneTask(taskB);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitFinished(CCircuitUnit* unit)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		DoneTask(iter->second);
	}
	auto itre = repairedUnits.find(unit->GetId());
	if (itre != repairedUnits.end()) {
		DoneTask(itre->second);
	}
	auto itcl = reclaimedUnits.find(unit);
	if (itcl != reclaimedUnits.end()) {
		AbortTask(itcl->second);
	}

	auto search = finishedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetCircuitDef()->GetId());
	if (search != idleHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	auto search = damagedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != damagedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		AbortTask(iter->second);
	}
	auto itre = repairedUnits.find(unit->GetId());
	if (itre != repairedUnits.end()) {
		AbortTask(itre->second);
	}
	auto itcl = reclaimedUnits.find(unit);
	if (itcl != reclaimedUnits.end()) {
		DoneTask(itcl->second);
	}

	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

void CBuilderManager::AddBuildPower(CCircuitUnit* unit)
{
	buildPower += unit->GetBuildSpeed();
	circuit->GetMilitaryManager()->AddResponse(unit);
}

void CBuilderManager::DelBuildPower(CCircuitUnit* unit)
{
	buildPower -= unit->GetBuildSpeed();
	circuit->GetMilitaryManager()->DelResponse(unit);
}

const std::set<IBuilderTask*>& CBuilderManager::GetTasks(IBuilderTask::BuildType type) const
{
	assert(type < IBuilderTask::BuildType::_SIZE_);
	return buildTasks[static_cast<IBuilderTask::BT>(type)];
}

void CBuilderManager::ActivateTask(IBuilderTask* task)
{
	if ((task->GetType() == IUnitTask::Type::BUILDER) && (task->GetBuildType() < IBuilderTask::BuildType::_SIZE_)) {
		buildTasks[static_cast<IBuilderTask::BT>(task->GetBuildType())].insert(task);
		buildTasksCount++;
	}
	buildUpdates.push_back(task);
	task->Activate();
}

IBuilderTask* CBuilderManager::EnqueueTask(IBuilderTask::Priority priority,
										   CCircuitDef* buildDef,
										   const AIFloat3& position,
										   IBuilderTask::BuildType type,
										   float cost,
										   float shake,
										   bool isActive,
										   int timeout)
{
	return AddTask(priority, buildDef, position, type, cost, shake, isActive, timeout);
}

IBuilderTask* CBuilderManager::EnqueueTask(IBuilderTask::Priority priority,
										   CCircuitDef* buildDef,
										   const AIFloat3& position,
										   IBuilderTask::BuildType type,
										   float shake,
										   bool isActive,
										   int timeout)
{
	float cost = buildDef->GetCost();
	return AddTask(priority, buildDef, position, type, cost, shake, isActive, timeout);
}

IBuilderTask* CBuilderManager::EnqueueFactory(IBuilderTask::Priority priority,
											  CCircuitDef* buildDef,
											  const AIFloat3& position,
											  float shake,
											  bool isPlop,
											  bool isActive,
											  int timeout)
{
	const float cost = isPlop ? 1.f : buildDef->GetCost();
	IBuilderTask* task = new CBFactoryTask(this, priority, buildDef, position, cost, shake, isPlop, timeout);
	if (isActive) {
		buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::FACTORY)].insert(task);
		buildTasksCount++;
		buildUpdates.push_back(task);
	} else {
		task->Deactivate();
	}
	return task;
}

IBuilderTask* CBuilderManager::EnqueuePylon(IBuilderTask::Priority priority,
											CCircuitDef* buildDef,
											const AIFloat3& position,
											CEnergyLink* link,
											float cost,
											bool isActive,
											int timeout)
{
	IBuilderTask* task = new CBPylonTask(this, priority, buildDef, position, link, cost, timeout);
	if (isActive) {
		buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::PYLON)].insert(task);
		buildTasksCount++;
		buildUpdates.push_back(task);
	} else {
		task->Deactivate();
	}
	return task;
}

IBuilderTask* CBuilderManager::EnqueueRepair(IBuilderTask::Priority priority,
											 CCircuitUnit* target,
											 int timeout)
{
	auto it = repairedUnits.find(target->GetId());
	if (it != repairedUnits.end()) {
		return it->second;
	}
	CBRepairTask* task = new CBRepairTask(this, priority, target, timeout);
	buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::REPAIR)].insert(task);
	buildTasksCount++;
	buildUpdates.push_back(task);
	repairedUnits[target->GetId()] = task;
	return task;
}

IBuilderTask* CBuilderManager::EnqueueReclaim(IBuilderTask::Priority priority,
											  const AIFloat3& position,
											  float cost,
											  int timeout,
											  float radius,
											  bool isMetal)
{
	IBuilderTask* task = new CBReclaimTask(this, priority, position, cost, timeout, radius, isMetal);
	buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::RECLAIM)].insert(task);
	buildTasksCount++;
	buildUpdates.push_back(task);
	return task;
}

IBuilderTask* CBuilderManager::EnqueueReclaim(IBuilderTask::Priority priority,
											  CCircuitUnit* target,
											  int timeout)
{
	auto it = reclaimedUnits.find(target);
	if (it != reclaimedUnits.end()) {
		return it->second;
	}
	CBReclaimTask* task = new CBReclaimTask(this, priority, target, timeout);
	buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::RECLAIM)].insert(task);
	buildTasksCount++;
	buildUpdates.push_back(task);
	reclaimedUnits[target] = task;
	return task;
}

IBuilderTask* CBuilderManager::EnqueuePatrol(IBuilderTask::Priority priority,
											 const AIFloat3& position,
											 float cost,
											 int timeout)
{
	IBuilderTask* task = new CBPatrolTask(this, priority, position, cost, timeout);
	buildUpdates.push_back(task);
	return task;
}

IBuilderTask* CBuilderManager::EnqueueTerraform(IBuilderTask::Priority priority,
												CCircuitUnit* target,
												const AIFloat3& position,
												float cost,
												bool isActive,
												int timeout)
{
	IBuilderTask* task;
	if (target == nullptr) {
		task = new CBTerraformTask(this, priority, position, cost, timeout);
	} else {
		task = new CBTerraformTask(this, priority, target, cost, timeout);
	}
	if (isActive) {
		buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::TERRAFORM)].insert(task);
		buildTasksCount++;
		buildUpdates.push_back(task);
	} else {
		task->Deactivate();
	}
	return task;
}

IBuilderTask* CBuilderManager::EnqueueGuard(IBuilderTask::Priority priority,
											CCircuitUnit* target,
											int timeout)
{
	IBuilderTask* task = new CBGuardTask(this, priority, target, timeout);
	buildUpdates.push_back(task);
	return task;
}

IUnitTask* CBuilderManager::EnqueueWait(int timeout)
{
	CBWaitTask* task = new CBWaitTask(this, timeout);
	buildUpdates.push_back(task);
	return task;
}

CRetreatTask* CBuilderManager::EnqueueRetreat()
{
	CRetreatTask* task = new CRetreatTask(this);
	buildUpdates.push_back(task);
	return task;
}

IBuilderTask* CBuilderManager::AddTask(IBuilderTask::Priority priority,
									   CCircuitDef* buildDef,
									   const AIFloat3& position,
									   IBuilderTask::BuildType type,
									   float cost,
									   float shake,
									   bool isActive,
									   int timeout)
{
	IBuilderTask* task;
	switch (type) {
		case IBuilderTask::BuildType::NANO: {
			task = new CBNanoTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::STORE: {
			task = new CBStoreTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::ENERGY: {
			task = new CBEnergyTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::DEFENCE: {
			task = new CBDefenceTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::BUNKER: {
			task = new CBBunkerTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		default:
		case IBuilderTask::BuildType::BIG_GUN: {
			task = new CBBigGunTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::RADAR: {
			task = new CBRadarTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::SONAR: {
			task = new CBSonarTask(this, priority, buildDef, position, cost, shake, timeout);
			break;
		}
		case IBuilderTask::BuildType::MEX: {
			task = new CBMexTask(this, priority, buildDef, position, cost, timeout);
			break;
		}
	}

	if (isActive) {
		buildTasks[static_cast<IBuilderTask::BT>(type)].insert(task);
		buildTasksCount++;
		buildUpdates.push_back(task);
	} else {
		task->Deactivate();
	}
	return task;
}

void CBuilderManager::DequeueTask(IBuilderTask* task, bool done)
{
	if ((task->GetType() == IUnitTask::Type::BUILDER) && (task->GetBuildType() < IBuilderTask::BuildType::_SIZE_)) {
		std::set<IBuilderTask*>& tasks = buildTasks[static_cast<IBuilderTask::BT>(task->GetBuildType())];
		auto it = tasks.find(task);
		if (it != tasks.end()) {
			switch (task->GetBuildType()) {
				case IBuilderTask::BuildType::REPAIR: {
					repairedUnits.erase(static_cast<CBRepairTask*>(task)->GetTargetId());
				} break;
				case IBuilderTask::BuildType::RECLAIM: {
					reclaimedUnits.erase(task->GetTarget());
				} break;
				default: {
					unfinishedUnits.erase(task->GetTarget());
				} break;
			}
			tasks.erase(it);
			buildTasksCount--;
		}
	}
	task->Dead();
	task->Close(done);
}

bool CBuilderManager::IsBuilderInArea(CCircuitDef* buildDef, const AIFloat3& position)
{
	if (!utils::is_valid(position)) {  // any-area task
		return true;
	}
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	for (auto& kv : buildAreas) {
		for (auto& kvw : kv.second) {
			if ((kvw.second > 0) && kvw.first->CanBuild(buildDef) &&
				terrainManager->CanMobileBuildAtUnsafe(kv.first, kvw.first, position))
			{
				return true;
			}
		}
	}
	return false;
}

IUnitTask* CBuilderManager::MakeTask(CCircuitUnit* unit)
{
	const CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->IsRoleComm()) {  // hide commander?
		const CSetupManager::SCommInfo::SHide* hide = circuit->GetSetupManager()->GetHide(cdef);
		if (hide != nullptr) {
			if ((circuit->GetLastFrame() < hide->frame) || (GetWorkerCount() <= 2)) {
				return MakeBuilderTask(unit);
			}
			CMilitaryManager* militaryManager = circuit->GetMilitaryManager();
			if (militaryManager->GetEnemyThreat() / circuit->GetAllyTeam()->GetAliveSize() >= hide->threat) {
				return MakeCommTask(unit);
			}
			const bool isHide = (hide->isAir) && (militaryManager->GetEnemyCost(CCircuitDef::RoleType::AIR) > 1.f);
			return isHide ? MakeCommTask(unit) : MakeBuilderTask(unit);
		}
	}

	return MakeBuilderTask(unit);
}

void CBuilderManager::AbortTask(IUnitTask* task)
{
	// NOTE: Don't send Stop command, save some traffic.
	DequeueTask(static_cast<IBuilderTask*>(task), false);
}

void CBuilderManager::DoneTask(IUnitTask* task)
{
	DequeueTask(static_cast<IBuilderTask*>(task), true);
}

void CBuilderManager::FallbackTask(CCircuitUnit* unit)
{
	DequeueTask(static_cast<IBuilderTask*>(unit->GetTask()));

	int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	IBuilderTask* task = EnqueuePatrol(IBuilderTask::Priority::LOW, pos, .0f, FRAMES_PER_SEC * 5);
	task->AssignTo(unit);
	task->Execute(unit);
}

SBuildChain* CBuilderManager::GetBuildChain(IBuilderTask::BuildType buildType, CCircuitDef* cdef)
{
	auto it1 = buildChains.find(static_cast<IBuilderTask::BT>(buildType));
	if (it1 == buildChains.end()) {
		return nullptr;
	}
	auto it2 = it1->second.find(cdef);
	if (it2 == it1->second.end()) {
		return nullptr;
	}
	return it2->second;
}

void CBuilderManager::ReadConfig()
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();

	terraDef = circuit->GetCircuitDef(root["economy"].get("terra", "").asCString());
	if (terraDef == nullptr) {
		terraDef = circuit->GetEconomyManager()->GetDefaultDef();
	}

	const Json::Value& cond = root["porcupine"]["condition"];
	super.minIncome = cond.get((unsigned)0, 50.f).asFloat();
	super.maxTime = cond.get((unsigned)1, 300.f).asFloat();

	IBuilderTask::BuildName& buildNames = IBuilderTask::GetBuildNames();
	const Json::Value& build = root["build_chain"];
	for (const std::string& catName : build.getMemberNames()) {
		auto it = buildNames.find(catName);
		if (it == buildNames.end()) {
			circuit->LOG("CONFIG %s: has unknown category '%s'", cfgName.c_str(), catName.c_str());
			continue;
		}

		std::unordered_map<CCircuitDef*, SBuildChain*>& defMap = buildChains[static_cast<IBuilderTask::BT>(it->second)];
		const Json::Value& catChain = build[catName];
		for (const std::string& defName : catChain.getMemberNames()) {
			CCircuitDef* cdef = circuit->GetCircuitDef(defName.c_str());
			if (cdef == nullptr) {
				circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), defName.c_str());
				continue;
			}

			SBuildChain bc;
			const Json::Value& buildQueue = catChain[defName];
			bc.isPylon = buildQueue.get("pylon", false).asBool();
			bc.isPorc = buildQueue.get("porc", false).asBool();
			bc.isTerra = buildQueue.get("terra", false).asBool();

			const Json::Value& engy = buildQueue["energy"];
			bc.energy = engy.get(unsigned(0), -1.f).asFloat();
			bc.isMexEngy = engy[1].isString();

			const Json::Value& hub = buildQueue["hub"];
			bc.hub.reserve(hub.size());
			for (const Json::Value& que : hub) {
				std::vector<SBuildInfo> queue;
				queue.reserve(que.size());
				for (const Json::Value& part : que) {
					SBuildInfo bi;

					const std::string& partName = part.get("unit", "").asString();
					bi.cdef = circuit->GetCircuitDef(partName.c_str());
					if (bi.cdef == nullptr) {
						circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), partName.c_str());
						continue;
					}

					const std::string& cat = part.get("category", "").asString();
					auto it = buildNames.find(cat);
					if (it == buildNames.end()) {
						circuit->LOG("CONFIG %s: has unknown category '%s'", cfgName.c_str(), cat.c_str());
						continue;
					}
					bi.buildType = it->second;

					UnitDef* unitDef = cdef->GetUnitDef();
					bi.offset = ZeroVector;
					bi.direction = SBuildInfo::Direction::NONE;
					const Json::Value& off = part["offset"];
					if (off.isArray()) {
						bi.offset = AIFloat3(off.get((unsigned)0, 0.f).asFloat(), 0.f, off.get((unsigned)1, 0.f).asFloat());
						if (bi.offset.x < -1e-3f) {
							bi.offset.x -= unitDef->GetXSize() * SQUARE_SIZE / 2;
						} else if (bi.offset.x > 1e-3f) {
							bi.offset.x += unitDef->GetXSize() * SQUARE_SIZE / 2;
						}
						if (bi.offset.z < -1e-3f) {
							bi.offset.z -= unitDef->GetZSize() * SQUARE_SIZE / 2;
						} else if (bi.offset.z > 1e-3f) {
							bi.offset.z += unitDef->GetZSize() * SQUARE_SIZE / 2;
						}
					} else if (off.isObject() && !off.empty()) {
						float delta = 0.f;
						SBuildInfo::DirName& dirNames = SBuildInfo::GetDirNames();
						std::string dir = off.getMemberNames().front();
						auto it = dirNames.find(dir);
						if (it != dirNames.end()) {
							bi.direction = it->second;
							delta = off[dir].asFloat();
						}
						switch (bi.direction) {
							case DIRECTION(LEFT): {
								bi.offset.x = delta + unitDef->GetXSize() * SQUARE_SIZE / 2;
							} break;
							case DIRECTION(RIGHT): {
								bi.offset.x = -(delta + unitDef->GetXSize() * SQUARE_SIZE / 2);
							} break;
							case DIRECTION(FRONT): {
								bi.offset.z = delta + unitDef->GetZSize() * SQUARE_SIZE / 2;
							} break;
							case DIRECTION(BACK): {
								bi.offset.z = -(delta + unitDef->GetZSize() * SQUARE_SIZE / 2);
							} break;
							default: break;
						}
					}

					bi.condition = SBuildInfo::Condition::ALWAYS;
					const std::string& cond = part.get("condition", "").asString();
					if (!cond.empty()) {
						SBuildInfo::CondName& condNames = SBuildInfo::GetCondNames();
						auto it = condNames.find(cond);
						if (it != condNames.end()) {
							bi.condition = it->second;
						}
					}

					queue.push_back(bi);
				}
				bc.hub.push_back(queue);
			}

			defMap[cdef] = new SBuildChain(bc);
		}
	}
}

void CBuilderManager::Init()
{
	CSetupManager::StartFunc subinit = [this](const AIFloat3& pos) {
		CScheduler* scheduler = circuit->GetScheduler().get();
		const int interval = 8;
		const int offset = circuit->GetSkirmishAIId() % interval;
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::UpdateIdle, this), interval, offset + 0);
		scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CBuilderManager::UpdateBuild, this), interval, offset + 1);
	};

	circuit->GetSetupManager()->ExecOnFindStart(subinit);
}

void CBuilderManager::Release()
{
	// NOTE: Release expected to be called on CCircuit::Release.
	//       It doesn't stop scheduled GameTasks for that reason.
	for (IUnitTask* task : buildUpdates) {
		AbortTask(task);
		// NOTE: Do not delete task as other AbortTask may ask for it
	}
	buildUpdates.clear();
}

IBuilderTask* CBuilderManager::MakeCommTask(CCircuitUnit* unit)
{
	circuit->GetThreatMap()->SetThreatType(unit);
	const IBuilderTask* task = nullptr;
	int frame = circuit->GetLastFrame();
	AIFloat3 pos = unit->GetPos(frame);

	CEconomyManager* economyManager = circuit->GetEconomyManager();
	economyManager->MakeEconomyTasks(pos, unit);
	const bool isNotReady = !economyManager->IsExcessed();

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CPathFinder* pathfinder = circuit->GetPathfinder();
//	CTerrainManager::CorrectPosition(pos);
	pathfinder->SetMapData(unit, circuit->GetThreatMap(), frame);
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float maxSpeed = cdef->GetSpeed() / pathfinder->GetSquareSize() * THREAT_BASE;
	const int buildDistance = std::max<int>(cdef->GetBuildDistance(), pathfinder->GetSquareSize());
	const AIFloat3& basePos = circuit->GetSetupManager()->GetBasePos();
	float metric = std::numeric_limits<float>::max();
	for (const std::set<IBuilderTask*>& tasks : buildTasks) {
		for (const IBuilderTask* candidate : tasks) {
			if (!candidate->CanAssignTo(unit) ||
				(isNotReady && (candidate->GetBuildDef() != nullptr) && (candidate->GetPriority() != IBuilderTask::Priority::NOW)))
			{
				continue;
			}

			// Check time-distance to target
			const AIFloat3& bp = candidate->GetPosition();
			AIFloat3 buildPos = utils::is_valid(bp) ? bp : pos;

			float distCost;
			if (candidate->GetPriority() == IBuilderTask::Priority::NOW) {
				// Disregard safety
				if (!terrainManager->CanBuildAtUnsafe(unit, buildPos)) {  // ensure that path always exists
					continue;
				}

				distCost = pathfinder->PathCost(pos, buildPos, buildDistance);

			} else {

				if ((basePos.SqDistance2D(buildPos) > SQUARE(2000.f)) ||  // FIXME: Make max distance configurable
					!terrainManager->CanBuildAt(unit, buildPos))  // ensure that path always exists
				{
					continue;
				}

				distCost = pathfinder->PathCostDirect(pos, buildPos, buildDistance);
				if (distCost < 0.0f) {
					continue;
				}
			}

			distCost = std::max(distCost, THREAT_BASE);

			float weight = (static_cast<float>(candidate->GetPriority()) + 1.0f);
			weight = 1.0f / SQUARE(weight);
			bool valid = false;

			CCircuitUnit* target = candidate->GetTarget();
			if ((target != nullptr) && (distCost * weight < metric)) {
				Unit* tu = target->GetUnit();
				const float maxHealth = tu->GetMaxHealth();
				const float health = tu->GetHealth() - maxHealth * 0.005f;
				const float healthSpeed = maxHealth * candidate->GetBuildPower() / candidate->GetCost();
				valid = ((maxHealth - health) * 0.6f) * (maxSpeed * FRAMES_PER_SEC) > healthSpeed * distCost;
			} else {
				valid = (distCost * weight < metric) && (distCost < MAX_TRAVEL_SEC * (maxSpeed * FRAMES_PER_SEC));
			}

			if (valid) {
				task = candidate;
				metric = distCost * weight;
			}
		}
	}

	if ((task == nullptr) &&
		((unit->GetTask()->GetType() != IUnitTask::Type::BUILDER) || (static_cast<IBuilderTask*>(unit->GetTask())->GetBuildType() != IBuilderTask::BuildType::GUARD)))
	{
		CCircuitUnit* vip = circuit->GetFactoryManager()->GetClosestFactory(pos);
		if (vip != nullptr) {
			task = EnqueueGuard(IBuilderTask::Priority::NORMAL, vip, FRAMES_PER_SEC * 60);
		} else {
			CCircuitDef* buildDef = circuit->GetMilitaryManager()->GetDefaultPorc();
			if ((buildDef != nullptr) && buildDef->IsAvailable()) {
				task = EnqueueTask(IBuilderTask::Priority::HIGH, buildDef, pos, IBuilderTask::BuildType::DEFENCE);
			}
		}
	}

	return const_cast<IBuilderTask*>(task);
}

IBuilderTask* CBuilderManager::MakeBuilderTask(CCircuitUnit* unit)
{
	CThreatMap* threatMap = circuit->GetThreatMap();
	threatMap->SetThreatType(unit);
	const IBuilderTask* task = nullptr;
	int frame = circuit->GetLastFrame();
	AIFloat3 pos = unit->GetPos(frame);

	CEconomyManager* economyManager = circuit->GetEconomyManager();
	task = economyManager->MakeEconomyTasks(pos, unit);
//	if (task != nullptr) {
//		return task;
//	}
	const bool isStalling = economyManager->IsMetalEmpty() &&
							(economyManager->GetAvgMetalIncome() * 1.2f < economyManager->GetMetalPull()) &&
							(metalPull > economyManager->GetPullMtoS() * circuit->GetFactoryManager()->GetMetalPull());
	const bool isNotReady = !economyManager->IsExcessed() || isStalling;

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CPathFinder* pathfinder = circuit->GetPathfinder();
//	CTerrainManager::CorrectPosition(pos);
	pathfinder->SetMapData(unit, threatMap, frame);
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float maxSpeed = cdef->GetSpeed() / pathfinder->GetSquareSize() * THREAT_BASE;
	const float maxThreat = threatMap->GetUnitThreat(unit);
	const int buildDistance = std::max<int>(cdef->GetBuildDistance(), pathfinder->GetSquareSize());
	float metric = std::numeric_limits<float>::max();
	for (const std::set<IBuilderTask*>& tasks : buildTasks) {
		for (const IBuilderTask* candidate : tasks) {
			if (!candidate->CanAssignTo(unit) || (isNotReady &&
												  (candidate->GetPriority() != IBuilderTask::Priority::NOW) &&
												  (candidate->GetBuildDef() != nullptr) &&
												  !economyManager->IsIgnorePull(candidate)))
			{
				continue;
			}

			// Check time-distance to target
			const AIFloat3& bp = candidate->GetPosition();
			AIFloat3 buildPos = utils::is_valid(bp) ? bp : pos;

			float distCost;
			if (candidate->GetPriority() >= IBuilderTask::Priority::HIGH) {
				// Disregard safety
				if (!terrainManager->CanBuildAtUnsafe(unit, buildPos)) {  // ensure that path always exists
					continue;
				}

				distCost = pathfinder->PathCost(pos, buildPos, buildDistance);

			} else {

				CCircuitDef* buildDef = candidate->GetBuildDef();
				const float buildThreat = (buildDef != nullptr) ? buildDef->GetPower() : 0.f;
				if ((threatMap->GetThreatAt(buildPos) > maxThreat + buildThreat) ||
					!terrainManager->CanBuildAtUnsafe(unit, buildPos))  // ensure that path always exists
				{
					continue;
				}

				// FIXME: Lags on large maps
				distCost = pathfinder->PathCostDirect(pos, buildPos, buildDistance);
				if (distCost < 0.0f) {
					continue;
				}
			}

			distCost = std::max(distCost, THREAT_BASE);

			float weight = (static_cast<float>(candidate->GetPriority()) + 1.0f);
			weight = 1.0f / SQUARE(weight);
			bool valid = false;

			CCircuitUnit* target = candidate->GetTarget();
			if (target != nullptr) {
				if (distCost * weight < metric) {
					// BA: float time_to_build = targetDef->GetBuildTime() / workerDef->GetBuildSpeed();
					Unit* tu = target->GetUnit();
					const float maxHealth = tu->GetMaxHealth();
					const float health = tu->GetHealth() - maxHealth * 0.005f;
					const float healthSpeed = maxHealth * candidate->GetBuildPower() / candidate->GetCost();
					valid = (((maxHealth - health) * 0.6f) * (maxSpeed * FRAMES_PER_SEC) > healthSpeed * distCost);
				}
			} else {
				valid = (distCost * weight < metric)/* && (distCost < MAX_TRAVEL_SEC * (maxSpeed * FRAMES_PER_SEC))*/;
			}

			if (valid) {
				task = candidate;
				metric = distCost * weight;
			}
		}
	}

	if (task == nullptr) {
		if (unit->GetTask() != idleTask) {
			return nullptr;  // current task is in danger or unreachable
		}
		task = CreateBuilderTask(pos, unit);
	}

	return const_cast<IBuilderTask*>(task);
}

IBuilderTask* CBuilderManager::CreateBuilderTask(const AIFloat3& position, CCircuitUnit* unit)
{
	CEconomyManager* em = circuit->GetEconomyManager();
	IBuilderTask* task = em->UpdateEnergyTasks(position, unit);
	if (task != nullptr) {
		return task;
	}
	task = em->UpdateReclaimTasks(position, unit, false);
//	if (task != nullptr) {
//		return task;
//	}

	// FIXME: Eco rules. It should never get here
	CCircuitDef* buildDef/* = nullptr*/;
	const float metalIncome = std::min(em->GetAvgMetalIncome(), em->GetAvgEnergyIncome()) * em->GetEcoFactor();
	if (metalIncome < super.minIncome) {
		float energyMake;
		buildDef = em->GetLowEnergy(position, energyMake);
		if (buildDef == nullptr) {  // position can be in danger
			buildDef = em->GetDefaultDef();
		}
		if ((buildDef->GetCount() < 10) && buildDef->IsAvailable()) {
			return EnqueueTask(IBuilderTask::Priority::NORMAL, buildDef, position, IBuilderTask::BuildType::ENERGY);
		}
	} else {
		CMilitaryManager* militaryManager = circuit->GetMilitaryManager();
		buildDef = militaryManager->GetBigGunDef();
		if ((buildDef != nullptr) && (buildDef->GetCost() < super.maxTime * metalIncome)) {
			const std::set<IBuilderTask*>& tasks = GetTasks(IBuilderTask::BuildType::BIG_GUN);
			if (tasks.empty()) {
				if (buildDef->IsAvailable() && militaryManager->IsNeedBigGun(buildDef)) {
					AIFloat3 pos = militaryManager->GetBigGunPos(buildDef);
					return EnqueueTask(IBuilderTask::Priority::NORMAL, buildDef, pos,
									   IBuilderTask::BuildType::BIG_GUN);
				}
			} else {
				return *tasks.begin();
			}
		}
	}

	return EnqueuePatrol(IBuilderTask::Priority::LOW, position, .0f, FRAMES_PER_SEC * 20);
}

void CBuilderManager::AddBuildList(CCircuitUnit* unit)
{
	CCircuitDef* cDef = unit->GetCircuitDef();
	if (cDef->GetCount() > 1) {
		return;
	}

	const std::unordered_set<CCircuitDef::Id>& buildOptions = cDef->GetBuildOptions();
	std::set<CCircuitDef*> buildDefs;
	for (CCircuitDef::Id build : buildOptions) {
		CCircuitDef* cdef = circuit->GetCircuitDef(build);
		if (cdef->GetBuildCount() == 0) {
			buildDefs.insert(cdef);
		}
		cdef->IncBuild();
	}

	if (!buildDefs.empty()) {
		circuit->GetEconomyManager()->AddEnergyDefs(buildDefs);
	}

	// TODO: Same thing with factory, etc.
}

void CBuilderManager::RemoveBuildList(CCircuitUnit* unit)
{
	CCircuitDef* cDef = unit->GetCircuitDef();
	if (cDef->GetCount() > 1) {
		return;
	}

	const std::unordered_set<CCircuitDef::Id>& buildOptions = cDef->GetBuildOptions();
	std::set<CCircuitDef*> buildDefs;
	for (CCircuitDef::Id build : buildOptions) {
		CCircuitDef* cdef = circuit->GetCircuitDef(build);
		cdef->DecBuild();
		if (cdef->GetBuildCount() == 0) {
			buildDefs.insert(cdef);
		}
	}

	if (!buildDefs.empty()) {  // throws exception on set::erase otherwise
		circuit->GetEconomyManager()->RemoveEnergyDefs(buildDefs);
	}

	// TODO: Same thing with factory, etc.
}

void CBuilderManager::Watchdog()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	Resource* metalRes = economyManager->GetMetalRes();
	// somehow workers get stuck
	for (CCircuitUnit* worker : workers) {
		if (worker->GetTask()->GetType() == IUnitTask::Type::PLAYER) {
			continue;
		}
		Unit* u = worker->GetUnit();
		auto commands = std::move(u->GetCurrentCommands());
		// TODO: Ignore workers with idle and wait task? (.. && worker->GetTask()->IsBusy())
		if (commands.empty() && (u->GetResourceUse(metalRes) == .0f) && (u->GetVel() == ZeroVector)) {
			worker->GetTask()->OnUnitMoveFailed(worker);
		}
		utils::free_clear(commands);
	}

	// find unfinished abandoned buildings
	float maxCost = MAX_BUILD_SEC * std::min(economyManager->GetAvgMetalIncome(), buildPower) * economyManager->GetEcoFactor();
	// TODO: Include special units
	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;
		if (!unit->GetCircuitDef()->IsMobile() &&
			(unfinishedUnits.find(unit) == unfinishedUnits.end()) &&
			(repairedUnits.find(unit->GetId()) == repairedUnits.end()) &&
			(reclaimedUnits.find(unit) == reclaimedUnits.end()))
		{
			Unit* u = unit->GetUnit();
			if (u->IsBeingBuilt()) {
				float maxHealth = u->GetMaxHealth();
				float buildPercent = (maxHealth - u->GetHealth()) / maxHealth;
				CCircuitDef* cdef = unit->GetCircuitDef();
				if ((cdef->GetBuildTime() * buildPercent < maxCost) || (*cdef == *terraDef)) {
					EnqueueRepair(IBuilderTask::Priority::NORMAL, unit);
				} else {
					EnqueueReclaim(IBuilderTask::Priority::NORMAL, unit);
				}
//			} else if (u->GetHealth() < u->GetMaxHealth()) {
//				EnqueueRepair(IBuilderTask::Priority::NORMAL, unit);
			}
		}
	}
}

void CBuilderManager::UpdateIdle()
{
	idleTask->Update();
}

void CBuilderManager::UpdateBuild()
{
	if (buildIterator >= buildUpdates.size()) {
		buildIterator = 0;
	}

	int lastFrame = circuit->GetLastFrame();
	// stagger the Update's
	unsigned int n = (buildUpdates.size() / TEAM_SLOWUPDATE_RATE) + 1;

	while ((buildIterator < buildUpdates.size()) && (n != 0)) {
		IUnitTask* task = buildUpdates[buildIterator];
		if (task->IsDead()) {
			buildUpdates[buildIterator] = buildUpdates.back();
			buildUpdates.pop_back();
			delete task;
		} else {
			int frame = task->GetLastTouched();
			int timeout = task->GetTimeout();
			if ((frame != -1) && (timeout > 0) && (lastFrame - frame >= timeout)) {
				AbortTask(task);
			} else {
				task->Update();
			}
			++buildIterator;
			n--;
		}
	}
}

void CBuilderManager::UpdateAreaUsers()
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	buildAreas.clear();
	for (auto mtId : workerMobileTypes) {
		for (auto& area : terrainManager->GetMobileTypeById(mtId)->area) {
			buildAreas[&area] = std::map<CCircuitDef*, int>();
		}
	}
	buildAreas[nullptr] = std::map<CCircuitDef*, int>();  // air

	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;
		if (workerDefs.find(unit->GetCircuitDef()) != workerDefs.end()) {
			++buildAreas[unit->GetArea()][unit->GetCircuitDef()];
		}
	}

	std::set<IBuilderTask*> removeTasks;
	for (auto& tasks : buildTasks) {
		for (IBuilderTask* task : tasks) {
			CCircuitDef* cdef = task->GetBuildDef();
			if ((cdef != nullptr) && !IsBuilderInArea(cdef, task->GetPosition())) {
				removeTasks.insert(task);
			}
		}
	}
	for (IBuilderTask* task : removeTasks) {
		AbortTask(task);
	}
}

void CBuilderManager::Load(std::istream& is)
{
//	std::streamsize y;
//	int yo, yoyo;
//	is >> y;
//	is.seekg(is.tellg() + std::streampos(sizeof('\n')));
//	is.read(reinterpret_cast<char*>(&yo), sizeof(yo));
//	is.read(reinterpret_cast<char*>(&yoyo), sizeof(yoyo));
//	circuit->LOG("LOAD ID: %i | %i | %i", y, yo, yoyo);

	/*
	 * Restore data
	 */
	for (IUnitTask* task : buildUpdates) {
		if (task->GetType() != IUnitTask::Type::BUILDER) {
			continue;
		}
		IBuilderTask* bt = static_cast<IBuilderTask*>(task);
		if (bt->GetBuildType() < IBuilderTask::BuildType::_SIZE_) {
			buildTasks[static_cast<IBuilderTask::BT>(bt->GetBuildType())].insert(bt);
			buildTasksCount++;
		}
	}
}

void CBuilderManager::Save(std::ostream& os) const
{
//	std::stringstream tmp;
//	int i = circuit->GetSkirmishAIId();
//	tmp.write(reinterpret_cast<const char*>(&i), sizeof(i));
//	i = 42;
//	tmp.write(reinterpret_cast<const char*>(&i), sizeof(i));
//	os << std::streamsize(1024) << '\n' << tmp.rdbuf();

	int size = buildUpdates.size();
	os.write(reinterpret_cast<const char*>(&size), sizeof(size));
	for (IUnitTask* task : buildUpdates) {
		os << task;
	}
	os.write(reinterpret_cast<const char*>(&buildIterator), sizeof(buildIterator));
}

} // namespace circuit
