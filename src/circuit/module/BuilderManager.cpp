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
#include "map/InfluenceMap.h"
#include "map/ThreatMap.h"
#include "resource/MetalManager.h"
#include "scheduler/Scheduler.h"
#include "script/BuilderScript.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/path/PathFinder.h"
#include "terrain/path/QueryCostMap.h"
#include "task/NilTask.h"
#include "task/IdleTask.h"
#include "task/RetreatTask.h"
#include "task/builder/GenericTask.h"
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
#include "task/builder/ResurrectTask.h"
#include "task/builder/PatrolTask.h"
#include "task/builder/GuardTask.h"
#include "task/builder/CombatTask.h"
#include "task/builder/BuildChain.h"
#include "CircuitAI.h"
#include "util/Utils.h"
#include "json/json.h"

#include "spring/SpringCallback.h"

#include "Log.h"
// FIXME: BA auto-mexer
#include "Sim/Units/CommandAI/Command.h"
#include "Command.h"
// FIXME: BA auto-mexer

namespace circuit {

using namespace springai;

CBuilderManager::CBuilderManager(CCircuitAI* circuit)
		: IUnitModule(circuit, new CBuilderScript(circuit->GetScriptManager(), this))
		, buildTasksCount(0)
		, buildPower(.0f)
		, buildIterator(0)
		, energizer1(nullptr)
		, energizer2(nullptr)
{
	circuit->GetScheduler()->RunOnInit(CScheduler::GameJob(&CBuilderManager::Init, this));

	/*
	 * worker handlers
	 */
	auto workerCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nilTask->AssignTo(unit);
			this->circuit->AddActionUnit(unit);
		}
		CEconomyManager* economyMgr = this->circuit->GetEconomyManager();
		if (unit->GetUnit()->IsBeingBuilt() && !economyMgr->IsEnergyStalling() && !economyMgr->IsMetalEmpty()) {
			EnqueueRepair(IBuilderTask::Priority::HIGH, unit);
		}
	};
	auto workerFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			this->circuit->AddActionUnit(unit);
		}
		CMilitaryManager* militaryMgr = this->circuit->GetMilitaryManager();
		// FIXME: BA
		if ((mexUpgrader[unit->GetCircuitDef()->GetMobileId()].size() < numAutoMex)
			&& unit->GetCircuitDef()->CanBuild(this->circuit->GetEconomyManager()->GetSideInfo().mohoMexDef))
		{
			IUnitTask* task = EnqueueWait(std::numeric_limits<int>::max());
			AssignTask(unit, task);

			AddBuildPower(unit);
			mexUpgrader[unit->GetCircuitDef()->GetMobileId()].insert(unit);
			++mexUpgraderCount[unit->GetCircuitDef()];

			TRY_UNIT(this->circuit, unit,
				unit->GetUnit()->ExecuteCustomCommand(CMD_AUTOMEX, {1.f});
			)
		} else {
			nilTask->RemoveAssignee(unit);
			idleTask->AssignTo(unit);

			++buildAreas[unit->GetArea()][unit->GetCircuitDef()];

			AddBuildPower(unit);
			workers.insert(unit);

			if (isBaseBuilderOn && !unit->GetCircuitDef()->IsRoleComm()) {
				if (unit->GetCircuitDef()->GetCostM() < 200) {
					if ((energizer1 == nullptr)
						&& (((unsigned)unit->GetCircuitDef()->GetCount() > militaryMgr->GetGuardTaskNum())
							|| unit->GetCircuitDef()->IsAbleToFly()))
					{
						energizer1 = unit;
						unit->AddAttribute(CCircuitDef::AttrType::BASE);
					}
				} else {
					if (energizer2 == nullptr) {
						energizer2 = unit;
						unit->AddAttribute(CCircuitDef::AttrType::BASE);
					}
				}
			}

			AddBuildList(unit, mexUpgraderCount[unit->GetCircuitDef()]);

			static_cast<CBuilderScript*>(script)->WorkerCreated(unit);
		}
		// FIXME: BA

		if (!unit->GetCircuitDef()->IsAttacker()
			&& !unit->GetCircuitDef()->IsAbleToFly()
			&& (militaryMgr->GetTasks(IFighterTask::FightType::GUARD).size() < militaryMgr->GetGuardTaskNum()))
		{
			militaryMgr->AddGuardTask(unit);
		}
	};
	auto workerIdleHandler = [this](CCircuitUnit* unit) {
		// FIXME: Avoid instant task reassignment, its only valid on build order fail.
		//        Can cause idle builder, but Watchdog should catch it eventually.
		//        Unfortunately can't use EVENT_COMMAND_FINISHED because there is no unique commandId like unitId
		if (this->circuit->GetLastFrame() > unit->GetTaskFrame()/* + FRAMES_PER_SEC*/) {
			unit->GetTask()->OnUnitIdle(unit);
		}
	};
	auto workerDamagedHandler = [](CCircuitUnit* unit, CEnemyInfo* attacker) {
		unit->GetTask()->OnUnitDamaged(unit, attacker);
	};
	auto workerDestroyedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		if (task->GetType() == IUnitTask::Type::NIL) {
			return;
		}
		// FIXME: BA
		if (mexUpgrader[unit->GetCircuitDef()->GetMobileId()].erase(unit) > 0) {
			DelBuildPower(unit);

			--mexUpgraderCount[unit->GetCircuitDef()];
		} else {
			--buildAreas[unit->GetArea()][unit->GetCircuitDef()];

			DelBuildPower(unit);
			workers.erase(unit);
			costQueries.erase(unit);

			if (isBaseBuilderOn) {
				if (energizer1 == unit) {
					energizer1 = nullptr;
				} else if (energizer2 == unit) {
					energizer2 = nullptr;
				}
			}

			RemoveBuildList(unit, mexUpgraderCount[unit->GetCircuitDef()]);

			static_cast<CBuilderScript*>(script)->WorkerDestroyed(unit);
		}
		// FIXME: BA

		this->circuit->GetMilitaryManager()->DelGuardTask(unit);
	};

	/*
	 * resurrect-bots
	 */
	auto rezzFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			this->circuit->AddActionUnit(unit);
		}
		nilTask->RemoveAssignee(unit);
		idleTask->AssignTo(unit);
		AddBuildPower(unit, false);
		workers.insert(unit);
	};
	auto rezzDestroyedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
		IUnitTask* task = unit->GetTask();
		task->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask

		if (task->GetType() == IUnitTask::Type::NIL) {
			return;
		}
		DelBuildPower(unit, false);
		workers.erase(unit);
		costQueries.erase(unit);
	};

	/*
	 * building handlers
	 */
	auto buildingDamagedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
		EnqueueRepair(IBuilderTask::Priority::HIGH, unit);
	};
	auto buildingDestroyedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
		int frame = this->circuit->GetLastFrame();
		int facing = unit->GetUnit()->GetBuildingFacing();
		this->circuit->GetTerrainManager()->DelBlocker(unit->GetCircuitDef(), unit->GetPos(frame), facing, true);
	};

	/*
	 * heavy handlers
	 */
//	auto heavyCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
//		CEconomyManager* economyMgr = this->circuit->GetEconomyManager();
//		if (economyMgr->GetAvgMetalIncome() * economyMgr->GetEcoFactor() > 32.0f) {
//			TRY_UNIT(this->circuit, unit,
//				unit->CmdPriority(2);
//			)
////			EnqueueRepair(IBuilderTask::Priority::LOW, unit);
//		}
//	};

	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const float builderRet = root["retreat"].get("builder", 0.8f).asFloat();

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	for (CCircuitDef& cdef : circuit->GetCircuitDefs()) {
		CCircuitDef::Id unitDefId = cdef.GetId();
		if (cdef.IsMobile()) {
			if (cdef.GetDef()->IsBuilder()) {
				if (!cdef.GetBuildOptions().empty()) {
					createdHandler[unitDefId]   = workerCreatedHandler;
					finishedHandler[unitDefId]  = workerFinishedHandler;
					idleHandler[unitDefId]      = workerIdleHandler;
					damagedHandler[unitDefId]   = workerDamagedHandler;
					destroyedHandler[unitDefId] = workerDestroyedHandler;

					int mtId = terrainMgr->GetMobileTypeId(unitDefId);
					if (mtId >= 0) {  // not air
						workerMobileTypes.insert(mtId);
					}
					workerDefs.insert(&cdef);
				} else if (cdef.IsAbleToResurrect()) {
					createdHandler[unitDefId]   = workerCreatedHandler;
					finishedHandler[unitDefId]  = rezzFinishedHandler;
					idleHandler[unitDefId]      = workerIdleHandler;
					damagedHandler[unitDefId]   = workerDamagedHandler;
					destroyedHandler[unitDefId] = rezzDestroyedHandler;
				}

				if (cdef.GetRetreat() < 0.f) {
					cdef.SetRetreat(builderRet);
				}
//			} else if (cdef->GetCostM() > 999.0f) {
//				createdHandler[unitDefId] = heavyCreatedHandler;
			}
		} else {
			damagedHandler[unitDefId]   = buildingDamagedHandler;
			destroyedHandler[unitDefId] = buildingDestroyedHandler;
		}
	}

	/*
	 * staticmex handlers;
	 */
	auto mexDestroyedHandler = [this](CCircuitUnit* unit, CEnemyInfo* attacker) {
		const AIFloat3& pos = unit->GetPos(this->circuit->GetLastFrame());
		CCircuitDef* mexDef = unit->GetCircuitDef();
		const int facing = unit->GetUnit()->GetBuildingFacing();
		this->circuit->GetTerrainManager()->DelBlocker(mexDef, pos, facing, true);
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
		this->circuit->GetScheduler()->RunJobAfter(CScheduler::GameJob([this, mexDef, pos, index]() {
			if (this->circuit->GetEconomyManager()->IsAllyOpenSpot(index) &&
				this->circuit->GetBuilderManager()->IsBuilderInArea(mexDef, pos) &&
				this->circuit->GetTerrainManager()->CanBeBuiltAtSafe(mexDef, pos))  // hostile environment
			{
				EnqueueTask(IBuilderTask::Priority::HIGH, mexDef, pos, IBuilderTask::BuildType::MEX)->SetBuildPos(pos);
				this->circuit->GetEconomyManager()->SetOpenSpot(index, false);
			}
		}), FRAMES_PER_SEC * 20);
	};
	for (const CEconomyManager::SSideInfo& si : circuit->GetEconomyManager()->GetSideInfos()) {
		destroyedHandler[si.mexDef->GetId()] = mexDestroyedHandler;
	}

	ReadConfig();

	buildTasks.resize(static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::_SIZE_));

	for (auto mtId : workerMobileTypes) {
		for (auto& area : terrainMgr->GetMobileTypeById(mtId)->area) {
			buildAreas[&area] = std::map<CCircuitDef*, int>();
		}
	}
	buildAreas[nullptr] = std::map<CCircuitDef*, int>();  // air
}

CBuilderManager::~CBuilderManager()
{
	for (IUnitTask* task : buildUpdates) {
		task->ClearRelease();
	}
	for (auto& kv1 : buildChains) {
		for (auto& kv2 : kv1.second) {
			delete kv2.second;
		}
	}
}

void CBuilderManager::ReadConfig()
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();

	const Json::Value& econ = root["economy"];
	terraDef = circuit->GetCircuitDef(econ.get("terra", "").asCString());
	if (terraDef == nullptr) {
		terraDef = circuit->GetEconomyManager()->GetSideInfo().defaultDef;
	}
	numAutoMex = econ.get("auto_mex", 2).asUInt();
	isBaseBuilderOn = econ.get("base_builder", true).asBool();

	const Json::Value& cond = root["porcupine"]["superweapon"]["condition"];
	super.minIncome = cond.get((unsigned)0, 50.f).asFloat();
	super.maxTime = cond.get((unsigned)1, 300.f).asFloat();

	SBuildInfo::DirName& dirNames = SBuildInfo::GetDirNames();
	SBuildInfo::CondName& condNames = SBuildInfo::GetCondNames();
	SBuildInfo::PrioName& prioNames = SBuildInfo::GetPrioNames();
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

					UnitDef* unitDef = cdef->GetDef();
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
					const Json::Value& condition = part["condition"];
					if (!condition.isNull()) {
						std::string cond = condition.getMemberNames().front();
						auto it = condNames.find(cond);
						if (it != condNames.end()) {
							bi.condition = it->second;
							const Json::Value& value = condition[cond];
							if (value.isBool()) {
								bi.value = value.asBool() ? 1.f : -1.f;
							} else {
								bi.value = condition.get(cond, 1.f).asFloat();
							}
						}
					}

					{
						const std::string& prio = part.get("priority", "").asString();
						auto it = prioNames.find(prio);
						bi.priority = (it != prioNames.end()) ? it->second : IBuilderTask::Priority::NORMAL;
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
		scheduler->RunJobEvery(CScheduler::GameJob(&CBuilderManager::UpdateIdle, this), interval, offset + 0);
		scheduler->RunJobEvery(CScheduler::GameJob(&CBuilderManager::UpdateBuild, this), 1/*interval*/, offset + 1);

		scheduler->RunJobEvery(CScheduler::GameJob(&CBuilderManager::Watchdog, this),
								FRAMES_PER_SEC * 60,
								circuit->GetSkirmishAIId() * WATCHDOG_COUNT + 10);
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
	for (IUnitTask* task : buildUpdates) {
		task->ClearRelease();
	}
	buildUpdates.clear();
}

int CBuilderManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetCircuitDef()->GetId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	if (builder == nullptr) {
		CTerrainManager* terrainMgr = circuit->GetTerrainManager();
		if (!unit->GetCircuitDef()->IsMobile() && !terrainMgr->ResignAllyBuilding(unit)) {
			// enemy unit captured
			const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
			terrainMgr->AddBlocker(unit->GetCircuitDef(), pos, unit->GetUnit()->GetBuildingFacing(), true);
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
		if ((taskB->GetTarget() == nullptr) && (taskB->GetBuildDef() != nullptr) &&
			(*taskB->GetBuildDef() == *unit->GetCircuitDef()) && taskB->IsEqualBuildPos(unit))
		{
#if 0
			AIFloat3 bp = taskB->GetBuildPos();
			circuit->GetDrawer()->AddPoint(bp, "bp");
			AIFloat3 up = unit->GetPos(circuit->GetLastFrame());
			circuit->GetDrawer()->AddPoint(up, "up");
			circuit->LOG("%s | bp = %f, %f | up = %f, %f", unit->GetCircuitDef()->GetDef()->GetName(), bp.x, bp.z, up.x, up.z);
#endif
			taskB->UpdateTarget(unit);
			unfinishedUnits[unit] = taskB;
		} else {
			// reclaim lost unit
			AssignTask(builder, EnqueueReclaim(IBuilderTask::Priority::HIGH, unit));
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
	auto itre = repairUnits.find(unit->GetId());
	if (itre != repairUnits.end()) {
		DoneTask(itre->second);
	}
	auto itcl = reclaimUnits.find(unit);
	if (itcl != reclaimUnits.end()) {
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

int CBuilderManager::UnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	auto search = damagedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != damagedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

int CBuilderManager::UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	auto iter = unfinishedUnits.find(unit);
	if (iter != unfinishedUnits.end()) {
		AbortTask(iter->second);
	}
	auto itre = repairUnits.find(unit->GetId());
	if (itre != repairUnits.end()) {
		AbortTask(itre->second);
	}
	auto itcl = reclaimUnits.find(unit);
	if (itcl != reclaimUnits.end()) {
		DoneTask(itcl->second);
	}

	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

void CBuilderManager::AddBuildPower(CCircuitUnit* unit, bool isBuilder)
{
	if (isBuilder) {
		buildPower += unit->GetBuildSpeed();
	}
	circuit->GetMilitaryManager()->AddResponse(unit);
}

void CBuilderManager::DelBuildPower(CCircuitUnit* unit, bool isBuilder)
{
	if (isBuilder) {
		buildPower -= unit->GetBuildSpeed();
	}
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
		switch (task->GetBuildType()) {
			case IBuilderTask::BuildType::FACTORY: {
				circuit->GetFactoryManager()->ApplySwitchFrame();
			} break;
			default: break;
		}
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
	float cost = buildDef->GetCostM();
	return AddTask(priority, buildDef, position, type, cost, shake, isActive, timeout);
}

IBuilderTask* CBuilderManager::EnqueueFactory(IBuilderTask::Priority priority,
											  CCircuitDef* buildDef,
											  CCircuitDef* reprDef,
											  const AIFloat3& position,
											  float shake,
											  bool isPlop,
											  bool isActive,
											  int timeout)
{
	const float cost = isPlop ? 1.f : buildDef->GetCostM();
	IBuilderTask* task = new CBFactoryTask(this, priority, buildDef, reprDef, position, cost, shake, isPlop, timeout);
	if (isActive) {
		buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::FACTORY)].insert(task);
		buildTasksCount++;
		buildUpdates.push_back(task);
		circuit->GetFactoryManager()->ApplySwitchFrame();
	} else {
		task->Deactivate();
	}
	TaskCreated(task);
	return task;
}

IBuilderTask* CBuilderManager::EnqueuePylon(IBuilderTask::Priority priority,
											CCircuitDef* buildDef,
											const AIFloat3& position,
											IGridLink* link,
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
	TaskCreated(task);
	return task;
}

IBuilderTask* CBuilderManager::EnqueueRepair(IBuilderTask::Priority priority,
											 CCircuitUnit* target,
											 int timeout)
{
	auto it = repairUnits.find(target->GetId());
	if (it != repairUnits.end()) {
		return it->second;
	}
	CBRepairTask* task = new CBRepairTask(this, priority, target, timeout);
	buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::REPAIR)].insert(task);
	buildTasksCount++;
	buildUpdates.push_back(task);
	repairUnits[target->GetId()] = task;
	TaskCreated(task);
	return task;
}

IBuilderTask* CBuilderManager::EnqueueReclaim(IBuilderTask::Priority priority,
											  const AIFloat3& position,
											  float cost,
											  int timeout,
											  float radius,
											  bool isMetal)
{
	CBReclaimTask* task = new CBReclaimTask(this, priority, position, cost, timeout, radius, isMetal);
	buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::RECLAIM)].insert(task);
	buildTasksCount++;
	buildUpdates.push_back(task);
	TaskCreated(task);
	return task;
}

IBuilderTask* CBuilderManager::EnqueueReclaim(IBuilderTask::Priority priority,
											  CCircuitUnit* target,
											  int timeout)
{
	auto it = reclaimUnits.find(target);
	if (it != reclaimUnits.end()) {
		return it->second;
	}
	CBReclaimTask* task = new CBReclaimTask(this, priority, target, timeout);
	buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::RECLAIM)].insert(task);
	buildTasksCount++;
	buildUpdates.push_back(task);
	reclaimUnits[target] = task;
	TaskCreated(task);
	return task;
}

IBuilderTask* CBuilderManager::EnqueueResurrect(IBuilderTask::Priority priority,
												const springai::AIFloat3& position,
												float cost,
												int timeout,
												float radius)
{
	CResurrectTask* task = new CResurrectTask(this, priority, position, cost, timeout, radius);
	buildTasks[static_cast<IBuilderTask::BT>(IBuilderTask::BuildType::RESURRECT)].insert(task);
	buildTasksCount++;
	buildUpdates.push_back(task);
	TaskCreated(task);
	return task;
}

IBuilderTask* CBuilderManager::EnqueuePatrol(IBuilderTask::Priority priority,
											 const AIFloat3& position,
											 float cost,
											 int timeout)
{
	IBuilderTask* task = new CBPatrolTask(this, priority, position, cost, timeout);
	buildUpdates.push_back(task);
	TaskCreated(task);
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
	TaskCreated(task);
	return task;
}

IBuilderTask* CBuilderManager::EnqueueGuard(IBuilderTask::Priority priority,
											CCircuitUnit* target,
											int timeout)
{
	IBuilderTask* task = new CBGuardTask(this, priority, target, timeout);
	buildUpdates.push_back(task);
	TaskCreated(task);
	return task;
}

IUnitTask* CBuilderManager::EnqueueWait(int timeout)
{
	CBWaitTask* task = new CBWaitTask(this, timeout);
	buildUpdates.push_back(task);
	TaskCreated(task);
	return task;
}

CRetreatTask* CBuilderManager::EnqueueRetreat()
{
	CRetreatTask* task = new CRetreatTask(this);
	buildUpdates.push_back(task);
	TaskCreated(task);
	return task;
}

CCombatTask* CBuilderManager::EnqueueCombat(float powerMod)
{
	CCombatTask* task = new CCombatTask(this, powerMod);
	buildUpdates.push_back(task);
	TaskCreated(task);
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
		default: {
			task = new CBGenericTask(this, type, priority, buildDef, position, cost, shake, timeout);
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
	TaskCreated(task);
	return task;
}

void CBuilderManager::DequeueTask(IUnitTask* task, bool done)
{
	switch (task->GetType()) {
		case IUnitTask::Type::BUILDER: {
			IBuilderTask* taskB = static_cast<IBuilderTask*>(task);
			if (taskB->GetBuildType() >= IBuilderTask::BuildType::_SIZE_) {
				break;
			}
			std::set<IBuilderTask*>& tasks = buildTasks[static_cast<IBuilderTask::BT>(taskB->GetBuildType())];
			auto it = tasks.find(taskB);
			if (it == tasks.end()) {
				break;
			}
			switch (taskB->GetBuildType()) {
				case IBuilderTask::BuildType::REPAIR: {
					repairUnits.erase(static_cast<CBRepairTask*>(taskB)->GetTargetId());
				} break;
				case IBuilderTask::BuildType::RECLAIM: {
					reclaimUnits.erase(taskB->GetTarget());
				} break;
				case IBuilderTask::BuildType::RESURRECT: {
				} break;
				default: {
					unfinishedUnits.erase(taskB->GetTarget());
				} break;
			}
			tasks.erase(it);
			buildTasksCount--;
		} break;
		default: break;
	}
	IUnitModule::DequeueTask(task, done);
}

void CBuilderManager::FallbackTask(CCircuitUnit* unit)
{
	DequeueTask(unit->GetTask());

	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	IBuilderTask* task = EnqueuePatrol(IBuilderTask::Priority::LOW, pos, .0f, FRAMES_PER_SEC * 5);
	task->AssignTo(unit);
	task->Start(unit);
}

bool CBuilderManager::IsBuilderInArea(CCircuitDef* buildDef, const AIFloat3& position) const
{
	if (!utils::is_valid(position)) {  // any-area task
		return true;
	}
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	for (auto& kv : buildAreas) {
		for (auto& kvw : kv.second) {
			if ((kvw.second > 0) && kvw.first->CanBuild(buildDef) &&
				terrainMgr->CanMobileReachAt(kv.first, position, kvw.first->GetBuildDistance()))
			{
				return true;
			}
		}
	}
	return false;
}

bool CBuilderManager::IsBuilderExists(CCircuitDef* buildDef) const
{
	for (const CCircuitUnit* builder : workers) {
		if (builder->GetCircuitDef()->CanBuild(buildDef->GetId())) {
			return true;
		}
	}
	return false;
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

IBuilderTask* CBuilderManager::GetReclaimFeatureTask(const AIFloat3& pos, float radius) const
{
	for (IBuilderTask* t : GetTasks(IBuilderTask::BuildType::RECLAIM)) {
		CBReclaimTask* rt = static_cast<CBReclaimTask*>(t);
		if ((rt->GetTarget() == nullptr) && rt->IsInRange(pos, radius)) {
			return rt;
		}
	}
	return nullptr;
}

IBuilderTask* CBuilderManager::GetResurrectTask(const AIFloat3& pos, float radius) const
{
	for (IBuilderTask* t : GetTasks(IBuilderTask::BuildType::RESURRECT)) {
		CResurrectTask* rt = static_cast<CResurrectTask*>(t);
		if (rt->IsInRange(pos, radius)) {
			return rt;
		}
	}
	return nullptr;
}

IUnitTask* CBuilderManager::DefaultMakeTask(CCircuitUnit* unit)
{
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);

	const CCircuitDef* cdef = unit->GetCircuitDef();
	if ((cdef->GetPower() > THREAT_MIN) && circuit->GetMilitaryManager()->IsCombatTargetExists(unit, pos, 1.5f)) {
		return EnqueueCombat(1.5f);
	}

	const auto it = costQueries.find(unit);
	std::shared_ptr<IPathQuery> query = (it == costQueries.end()) ? nullptr : it->second;
	if ((query != nullptr) && (query->GetState() != IPathQuery::State::READY)) {  // not ready
		return nullptr;
	}

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> q = pathfinder->CreateCostMapQuery(unit, circuit->GetThreatMap(), frame,
			/*unit->IsAttrBase() ? circuit->GetSetupManager()->GetBasePos() : */pos);
	costQueries[unit] = q;
	pathfinder->RunQuery(q);

	if (query == nullptr) {
		return EnqueueWait(FRAMES_PER_SEC);  // 1st run
	}

	std::shared_ptr<CQueryCostMap> pQuery = std::static_pointer_cast<CQueryCostMap>(query);

	if (cdef->IsRoleComm() && (circuit->GetFactoryManager()->GetFactoryCount() > 0)) {  // hide commander?
		CEnemyManager* enemyMgr = circuit->GetEnemyManager();
		const CSetupManager::SCommInfo::SHide* hide = circuit->GetSetupManager()->GetHide(cdef);
		if (hide != nullptr) {
			if ((frame < hide->frame) || (GetWorkerCount() <= 2)) {
				return (hide->sqPeaceTaskRad < 0.f)
						? MakeBuilderTask(unit, pQuery.get())
						: MakeCommPeaceTask(unit, pQuery.get(), hide->sqPeaceTaskRad);
			}
//			if ((enemyMgr->GetMobileThreat() / circuit->GetAllyTeam()->GetAliveSize() >= hide->threat)
			if ((circuit->GetInflMap()->GetEnemyInflAt(pos) >= hide->threat)
				|| ((hide->isAir) && (enemyMgr->GetEnemyCost(ROLE_TYPE(AIR)) > 1.f)))
			{
				return MakeCommDangerTask(unit, pQuery.get(), hide->sqDangerTaskRad);
			}
			return (hide->sqPeaceTaskRad < 0.f)
					? MakeBuilderTask(unit, pQuery.get())
					: MakeCommPeaceTask(unit, pQuery.get(), hide->sqPeaceTaskRad);
		}
	}

	return unit->IsAttrBase()
			? MakeEnergizerTask(unit, pQuery.get())
			: MakeBuilderTask(unit, pQuery.get());
}

IBuilderTask* CBuilderManager::MakeEnergizerTask(CCircuitUnit* unit, const CQueryCostMap* query)
{
	if (GetTasks(IBuilderTask::BuildType::STORE).empty()
		&& GetTasks(IBuilderTask::BuildType::ENERGY).empty()
		&& GetTasks(IBuilderTask::BuildType::FACTORY).empty()
		&& GetTasks(IBuilderTask::BuildType::NANO).empty())
	{
		return MakeCommPeaceTask(unit, query, SQUARE(2000));
	}

	CThreatMap* threatMap = circuit->GetThreatMap();
	threatMap->SetThreatType(unit);
	const IBuilderTask* task = nullptr;
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
//	CTerrainManager::CorrectPosition(pos);

	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	economyMgr->MakeEconomyTasks(pos, unit);
	// TODO: Add excess mechanics for lower difficulties, @see MakeBuilderTask

	CMetalManager* metalMgr = circuit->GetMetalManager();
	const CMetalData::Clusters& clusters = metalMgr->GetClusters();

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CInfluenceMap* inflMap = circuit->GetInflMap();
	CPathFinder* pathfinder = circuit->GetPathfinder();

	CCircuitDef* cdef = unit->GetCircuitDef();
	const float maxSpeed = cdef->GetSpeed() / pathfinder->GetSquareSize() * COST_BASE;
	const int buildDistance = std::max<int>(cdef->GetBuildDistance(), pathfinder->GetSquareSize());
	float metric = std::numeric_limits<float>::max();
	for (const std::set<IBuilderTask*>& tasks : buildTasks) {
		for (const IBuilderTask* candidate : tasks) {
			if (!candidate->CanAssignTo(unit)) {
				continue;
			}
			float prioMod = 1.f;
			switch (candidate->GetBuildType()) {
				case IBuilderTask::BuildType::NANO:
				case IBuilderTask::BuildType::FACTORY: {
					prioMod = .0001f;
				} break;
				case IBuilderTask::BuildType::STORE: {
					prioMod = .001f;
				} break;
				case IBuilderTask::BuildType::ENERGY: {
					prioMod = 1000.f;
				} break;
				default: continue;
			}

			// Check time-distance to target
			const AIFloat3& bp = candidate->GetPosition();
			AIFloat3 buildPos = utils::is_valid(bp) ? bp : pos;

			if (candidate->GetPriority() == IBuilderTask::Priority::NOW) {
				// Disregard safety
				if (!terrainMgr->CanReachAt(unit, buildPos, cdef->GetBuildDistance())) {  // ensure that path always exists
					continue;
				}

			} else {

				int index = metalMgr->FindNearestCluster(buildPos);
				const AIFloat3& testPos = (index < 0) ? buildPos : clusters[index].position;
				if (!terrainMgr->CanReachAt(unit, buildPos, cdef->GetBuildDistance())  // ensure that path always exists
					|| (inflMap->GetInfluenceAt(testPos) < -INFL_EPS))  // safety check
				{
					continue;
				}
			}

			float distCost;
			const float rawDist = pos.SqDistance2D(buildPos);
			if (rawDist < buildDistance) {
				distCost = rawDist / pathfinder->GetSquareSize() * COST_BASE;
			} else {
				distCost = query->GetCostAt(buildPos, buildDistance);
				if (distCost < 0.f) {  // path blocked by buildings
					continue;
				}
			}

			distCost = std::max(distCost, COST_BASE);

			float weight = (static_cast<float>(candidate->GetPriority()) + 1.0f);
			weight = 1.0f / SQUARE(weight) * prioMod;
			bool valid = false;

			CCircuitUnit* target = candidate->GetTarget();
			if ((target != nullptr) && (distCost * weight < metric)) {
				Unit* tu = target->GetUnit();
				const float maxHealth = tu->GetMaxHealth();
				const float health = tu->GetHealth() - maxHealth * 0.005f;
				const float healthSpeed = maxHealth * candidate->GetBuildPower() / candidate->GetCost();
				valid = ((maxHealth - health) * 0.6f) * maxSpeed > healthSpeed * distCost;
			} else {
				valid = (distCost * weight < metric) && (distCost < MAX_TRAVEL_SEC * maxSpeed);
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
			task = EnqueueGuard(IBuilderTask::Priority::NORMAL, vip, FRAMES_PER_SEC * 10);
		} else {
			task = EnqueuePatrol(IBuilderTask::Priority::LOW, pos, .0f, FRAMES_PER_SEC * 5);
		}
	}

	return const_cast<IBuilderTask*>(task);
}

IBuilderTask* CBuilderManager::MakeCommPeaceTask(CCircuitUnit* unit, const CQueryCostMap* query, float sqMaxBaseRange)
{
	CThreatMap* threatMap = circuit->GetThreatMap();
	threatMap->SetThreatType(unit);
	const IBuilderTask* task = nullptr;
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
//	CTerrainManager::CorrectPosition(pos);

	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	economyMgr->MakeEconomyTasks(pos, unit);
	const bool isNotReady = !economyMgr->IsExcessed();

	CMetalManager* metalMgr = circuit->GetMetalManager();
	const CMetalData::Clusters& clusters = metalMgr->GetClusters();

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CInfluenceMap* inflMap = circuit->GetInflMap();
	CPathFinder* pathfinder = circuit->GetPathfinder();

	CCircuitDef* cdef = unit->GetCircuitDef();
	const float maxSpeed = cdef->GetSpeed() / pathfinder->GetSquareSize() * COST_BASE;
	const int buildDistance = std::max<int>(cdef->GetBuildDistance(), pathfinder->GetSquareSize());
	const AIFloat3& basePos = circuit->GetSetupManager()->GetBasePos();
	float metric = std::numeric_limits<float>::max();
	for (const std::set<IBuilderTask*>& tasks : buildTasks) {
		for (const IBuilderTask* candidate : tasks) {
			if (!candidate->CanAssignTo(unit)
				|| (isNotReady
					&& (candidate->GetBuildDef() != nullptr)
					&& (candidate->GetPriority() != IBuilderTask::Priority::NOW)))
			{
				continue;
			}

			// Check time-distance to target
			const AIFloat3& bp = candidate->GetPosition();
			AIFloat3 buildPos = utils::is_valid(bp) ? bp : pos;

			if (candidate->GetPriority() == IBuilderTask::Priority::NOW) {
				// Disregard safety
				if (!terrainMgr->CanReachAt(unit, buildPos, cdef->GetBuildDistance())) {  // ensure that path always exists
					continue;
				}

			} else {

				int index = metalMgr->FindNearestCluster(buildPos);
				const AIFloat3& testPos = (index < 0) ? buildPos : clusters[index].position;
				if ((basePos.SqDistance2D(testPos) > sqMaxBaseRange)
					|| !terrainMgr->CanReachAt(unit, buildPos, cdef->GetBuildDistance())  // ensure that path always exists
					|| (inflMap->GetInfluenceAt(testPos) < -INFL_EPS))  // safety check
				{
					continue;
				}
			}

			float distCost;
			const float rawDist = pos.SqDistance2D(buildPos);
			if (rawDist < buildDistance) {
				distCost = rawDist / pathfinder->GetSquareSize() * COST_BASE;
			} else {
				distCost = query->GetCostAt(buildPos, buildDistance);
				if (distCost < 0.f) {  // path blocked by buildings
					continue;
				}
			}

			distCost = std::max(distCost, COST_BASE);

			float weight = (static_cast<float>(candidate->GetPriority()) + 1.0f);
			weight = 1.0f / SQUARE(weight);
			bool valid = false;

			CCircuitUnit* target = candidate->GetTarget();
			if ((target != nullptr) && (distCost * weight < metric)) {
				Unit* tu = target->GetUnit();
				const float maxHealth = tu->GetMaxHealth();
				const float health = tu->GetHealth() - maxHealth * 0.005f;
				const float healthSpeed = maxHealth * candidate->GetBuildPower() / candidate->GetCost();
				valid = ((maxHealth - health) * 0.6f) * maxSpeed > healthSpeed * distCost;
			} else {
				valid = (distCost * weight < metric) && (distCost < MAX_TRAVEL_SEC * maxSpeed);
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
			task = EnqueuePatrol(IBuilderTask::Priority::LOW, pos, .0f, FRAMES_PER_SEC * 5);
		}
	}

	return const_cast<IBuilderTask*>(task);
}

IBuilderTask* CBuilderManager::MakeCommDangerTask(CCircuitUnit* unit, const CQueryCostMap* query, float sqMaxBaseRange)
{
	CThreatMap* threatMap = circuit->GetThreatMap();
	threatMap->SetThreatType(unit);
	const IBuilderTask* task = nullptr;
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
//	CTerrainManager::CorrectPosition(pos);

	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	economyMgr->MakeEconomyTasks(pos, unit);
	const bool isNotReady = !economyMgr->IsExcessed();

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CInfluenceMap* inflMap = circuit->GetInflMap();
	CPathFinder* pathfinder = circuit->GetPathfinder();

	CCircuitDef* cdef = unit->GetCircuitDef();
	const float maxSpeed = cdef->GetSpeed() / pathfinder->GetSquareSize() * COST_BASE;
	const int buildDistance = std::max<int>(cdef->GetBuildDistance(), pathfinder->GetSquareSize());
	const AIFloat3& basePos = circuit->GetSetupManager()->GetBasePos();
	float metric = std::numeric_limits<float>::max();
	for (const std::set<IBuilderTask*>& tasks : buildTasks) {
		for (const IBuilderTask* candidate : tasks) {
			if (!candidate->CanAssignTo(unit)
				|| (isNotReady
					&& (candidate->GetBuildDef() != nullptr)
					&& (candidate->GetPriority() != IBuilderTask::Priority::NOW)))
			{
				continue;
			}

			// Check time-distance to target
			const AIFloat3& bp = candidate->GetPosition();
			AIFloat3 buildPos = utils::is_valid(bp) ? bp : pos;

			if (candidate->GetPriority() == IBuilderTask::Priority::NOW) {
				// Disregard safety
				if (!terrainMgr->CanReachAt(unit, buildPos, cdef->GetBuildDistance())) {  // ensure that path always exists
					continue;
				}

			} else {

				if ((basePos.SqDistance2D(buildPos) > sqMaxBaseRange)
					|| !terrainMgr->CanReachAtSafe(unit, buildPos, cdef->GetBuildDistance())  // ensure that path always exists
					|| (inflMap->GetEnemyInflAt(buildPos) > INFL_EPS))  // safety check
				{
					continue;
				}
			}

			float distCost;
			const float rawDist = pos.SqDistance2D(buildPos);
			if (rawDist < buildDistance) {
				distCost = rawDist / pathfinder->GetSquareSize() * COST_BASE;
			} else {
				distCost = query->GetCostAt(buildPos, buildDistance);
				if (distCost < 0.f) {  // path blocked by buildings
					continue;
				}
			}

			distCost = std::max(distCost, COST_BASE);

			float weight = (static_cast<float>(candidate->GetPriority()) + 1.0f);
			weight = 1.0f / SQUARE(weight);
			bool valid = false;

			CCircuitUnit* target = candidate->GetTarget();
			if ((target != nullptr) && (distCost * weight < metric)) {
				Unit* tu = target->GetUnit();
				const float maxHealth = tu->GetMaxHealth();
				const float health = tu->GetHealth() - maxHealth * 0.005f;
				const float healthSpeed = maxHealth * candidate->GetBuildPower() / candidate->GetCost();
				valid = ((maxHealth - health) * 0.6f) * maxSpeed > healthSpeed * distCost;
			} else {
				valid = (distCost * weight < metric) && (distCost < MAX_TRAVEL_SEC * maxSpeed);
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
			task = EnqueuePatrol(IBuilderTask::Priority::LOW, pos, .0f, FRAMES_PER_SEC * 5);
		}
	}

	return const_cast<IBuilderTask*>(task);
}

IBuilderTask* CBuilderManager::MakeBuilderTask(CCircuitUnit* unit, const CQueryCostMap* query)
{
	CThreatMap* threatMap = circuit->GetThreatMap();
	threatMap->SetThreatType(unit);
	const IBuilderTask* task = nullptr;
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
//	CTerrainManager::CorrectPosition(pos);

	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	task = economyMgr->MakeEconomyTasks(pos, unit);
//	if (task != nullptr) {
//		return const_cast<IBuilderTask*>(task);
//	}
	const bool isStalling = economyMgr->IsMetalEmpty() &&
							(economyMgr->GetAvgMetalIncome() * 1.2f < economyMgr->GetMetalPull()) &&
							(metalPull > economyMgr->GetPullMtoS() * circuit->GetFactoryManager()->GetMetalPull());
	const bool isNotReady = !economyMgr->IsExcessed() || isStalling/* || economyMgr->IsEnergyStalling()*/;

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CInfluenceMap* inflMap = circuit->GetInflMap();
	CPathFinder* pathfinder = circuit->GetPathfinder();

	CCircuitDef* cdef = unit->GetCircuitDef();
	const float maxSpeed = cdef->GetSpeed() / pathfinder->GetSquareSize() * COST_BASE;
	const float maxThreat = threatMap->GetUnitPower(unit);
	const int buildDistance = std::max<int>(cdef->GetBuildDistance(), pathfinder->GetSquareSize());
	float metric = std::numeric_limits<float>::max();
	for (const std::set<IBuilderTask*>& tasks : buildTasks) {
		for (const IBuilderTask* candidate : tasks) {
			if (!candidate->CanAssignTo(unit)
				|| (isNotReady
					&& (candidate->GetPriority() != IBuilderTask::Priority::NOW)
					&& (candidate->GetBuildDef() != nullptr)
					&& !economyMgr->IsIgnoreStallingPull(candidate)))
			{
				continue;
			}

			// Check time-distance to target
			const AIFloat3& bp = candidate->GetPosition();
			AIFloat3 buildPos = utils::is_valid(bp) ? bp : pos;

			if (candidate->GetPriority() == IBuilderTask::Priority::NOW) {
				// Disregard safety
				if (!terrainMgr->CanReachAt(unit, buildPos, cdef->GetBuildDistance())) {  // ensure that path always exists
					continue;
				}

			} else {

				CCircuitDef* buildDef = candidate->GetBuildDef();
				const float buildThreat = (buildDef != nullptr) ? buildDef->GetPower() : 0.f;
				if (!terrainMgr->CanReachAt(unit, buildPos, cdef->GetBuildDistance())  // ensure that path always exists
					|| (((buildThreat < THREAT_MIN) && (threatMap->GetThreatAt(buildPos) > maxThreat))
						&& (inflMap->GetInfluenceAt(buildPos) < -INFL_EPS)))  // safety check
				{
					continue;
				}
			}

			float distCost;
			const float rawDist = pos.SqDistance2D(buildPos);
			if (rawDist < buildDistance) {
				distCost = rawDist / pathfinder->GetSquareSize() * COST_BASE;
			} else {
				distCost = query->GetCostAt(buildPos, buildDistance);
				if (distCost < 0.f) {  // path blocked by buildings
					continue;
				}
			}

			distCost = std::max(distCost, COST_BASE);

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
					valid = (((maxHealth - health) * 0.6f) * maxSpeed > healthSpeed * distCost);
				}
			} else {
				valid = (distCost * weight < metric)/* && (distCost < MAX_TRAVEL_SEC * maxSpeed)*/;
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
	CEconomyManager* ecoMgr = circuit->GetEconomyManager();
	IBuilderTask* task = ecoMgr->UpdateEnergyTasks(position, unit);
	if (task != nullptr) {
		return task;
	}
	task = ecoMgr->UpdateReclaimTasks(position, unit, false);
//	if (task != nullptr) {
//		return task;
//	}

	// FIXME: Eco rules. It should never get here
	CMilitaryManager* militaryMgr = circuit->GetMilitaryManager();
	CCircuitDef* buildDef/* = nullptr*/;
	const float energyIncome = ecoMgr->GetAvgEnergyIncome() * ecoMgr->GetEcoFactor();
	const float metalIncome = std::min(ecoMgr->GetAvgMetalIncome() * ecoMgr->GetEcoFactor(), energyIncome);
	if ((metalIncome < super.minIncome) || (energyIncome * 0.05f < super.minIncome)) {
		float energyMake;
		buildDef = ecoMgr->GetLowEnergy(position, energyMake, unit);
		if (buildDef == nullptr) {  // position can be in danger
			buildDef = ecoMgr->GetDefaultDef(unit->GetCircuitDef());
		}
		if ((buildDef != nullptr) && (buildDef->GetCount() < 10)
			&& buildDef->IsAvailable(circuit->GetLastFrame())
			&& unit->GetCircuitDef()->CanBuild(buildDef))
		{
			return EnqueueTask(IBuilderTask::Priority::NORMAL, buildDef, position, IBuilderTask::BuildType::ENERGY);
		}
	} else {
		buildDef = militaryMgr->GetBigGunDef();
		if ((buildDef != nullptr) && (buildDef->GetCostM() < super.maxTime * metalIncome)) {
			const std::set<IBuilderTask*>& tasks = GetTasks(IBuilderTask::BuildType::BIG_GUN);
			if (tasks.empty()) {
				if (buildDef->IsAvailable(circuit->GetLastFrame())
					&& militaryMgr->IsNeedBigGun(buildDef)
					&& unit->GetCircuitDef()->CanBuild(buildDef))
				{
					AIFloat3 pos = militaryMgr->GetBigGunPos(buildDef);
					return EnqueueTask(IBuilderTask::Priority::NORMAL, buildDef, pos,
									   IBuilderTask::BuildType::BIG_GUN);
				}
			} else if (unit->GetCircuitDef()->CanBuild(buildDef)) {
				return *tasks.begin();
			}
		}
	}

	CSetupManager* setupMgr = circuit->GetSetupManager();
	if ((setupMgr->GetCommander() != nullptr) && !unit->GetCircuitDef()->IsAbleToAssist()) {
		return EnqueueGuard(IBuilderTask::Priority::LOW, setupMgr->GetCommander(), FRAMES_PER_SEC * 20);
	}
	return EnqueuePatrol(IBuilderTask::Priority::LOW, position, .0f, FRAMES_PER_SEC * 20);
}

void CBuilderManager::AddBuildList(CCircuitUnit* unit, int hiddenDefs)
{
	CCircuitDef* cDef = unit->GetCircuitDef();
	if (cDef->GetCount() - hiddenDefs > 1) {
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
		circuit->GetEconomyManager()->AddEconomyDefs(buildDefs);
		circuit->GetMilitaryManager()->AddSensorDefs(buildDefs);
	}

	// TODO: Same thing with factory, currently factory uses IsBuilderExists
}

void CBuilderManager::RemoveBuildList(CCircuitUnit* unit, int hiddenDefs)
{
	CCircuitDef* cDef = unit->GetCircuitDef();
	if (cDef->GetCount() - hiddenDefs > 1) {
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
		circuit->GetEconomyManager()->RemoveEconomyDefs(buildDefs);
		circuit->GetMilitaryManager()->RemoveSensorDefs(buildDefs);
	}

	// TODO: Same thing with factory, etc.
}

void CBuilderManager::Watchdog()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	Resource* metalRes = economyMgr->GetMetalRes();
	// somehow workers get stuck
	for (CCircuitUnit* worker : workers) {
		if ((worker->GetTask()->GetType() == IUnitTask::Type::PLAYER)
			|| (worker->GetTask()->GetType() == IUnitTask::Type::WAIT))
		{
			continue;
		}
		Unit* u = worker->GetUnit();
		// TODO: Ignore workers with idle and wait task? (.. && worker->GetTask()->IsBusy())
		if (!circuit->GetCallback()->Unit_HasCommands(worker->GetId())
			|| ((u->GetResourceUse(metalRes) < 1e-3f) && (u->GetVel().SqLength2D() < 1e-3f) && !worker->IsWaiting()))
		{
			worker->GetTask()->OnUnitMoveFailed(worker);
		}
	}
	for (const auto& kv : mexUpgrader) {  // FIXME: autoMexer stuck on reclaiming mobile unit
		for (CCircuitUnit* unit : kv.second) {
			if (unit->GetTask()->GetType() != IUnitTask::Type::WAIT) {
				continue;
			}
			ICoreUnit::Id reclId = unit->GetUnitIdReclaim();
			if (reclId == -1) {
				continue;
			}
			CAllyUnit* au = circuit->GetFriendlyUnit(reclId);
			if ((au == nullptr) || au->GetCircuitDef()->IsMobile()) {
				auto commands = unit->GetUnit()->GetCurrentCommands();
				std::vector<float> tags;
				tags.reserve(commands.size());
				for (springai::Command* cmd : commands) {
					if (cmd->GetId() == CMD_RECLAIM) {
						CAllyUnit* au = circuit->GetFriendlyUnit(cmd->GetParams()[0]);
						if ((au == nullptr) || au->GetCircuitDef()->IsMobile()) {
							tags.push_back(cmd->GetTag());
						}
					}
					delete cmd;
				}
				TRY_UNIT(circuit, unit,
					unit->CmdRemove(std::move(tags));
				)
			}
		}
	}

	// find unfinished abandoned buildings
	float maxCost = MAX_BUILD_SEC * std::min(economyMgr->GetAvgMetalIncome(), buildPower) * economyMgr->GetEcoFactor();
	// TODO: Include special units
	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;
		if (!unit->GetCircuitDef()->IsMobile() &&
			(unfinishedUnits.find(unit) == unfinishedUnits.end()) &&
			(repairUnits.find(unit->GetId()) == repairUnits.end()) &&
			(reclaimUnits.find(unit) == reclaimUnits.end()))
		{
			Unit* u = unit->GetUnit();
			if (u->IsBeingBuilt()) {
				float maxHealth = u->GetMaxHealth();
				float buildPercent = (maxHealth - u->GetHealth()) / maxHealth;
				CCircuitDef* cdef = unit->GetCircuitDef();
				if ((cdef->GetBuildTime() * buildPercent < maxCost) || (*cdef == *terraDef)
						|| (*cdef == *economyMgr->GetSideInfo().mohoMexDef))  // FIXME: BA
				{
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
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	idleTask->Update();
}

void CBuilderManager::UpdateBuild()
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
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
			task->ClearRelease();  // delete task;
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
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	buildAreas.clear();
	for (auto mtId : workerMobileTypes) {
		for (auto& area : terrainMgr->GetMobileTypeById(mtId)->area) {
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
			if ((cdef != nullptr) && (task->GetTarget() == nullptr) && !IsBuilderInArea(cdef, task->GetPosition())) {
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
//	for (IUnitTask* task : buildUpdates) {
//		if (task->GetType() != IUnitTask::Type::BUILDER) {
//			continue;
//		}
//		IBuilderTask* bt = static_cast<IBuilderTask*>(task);
//		if (bt->GetBuildType() < IBuilderTask::BuildType::_SIZE_) {
//			buildTasks[static_cast<IBuilderTask::BT>(bt->GetBuildType())].insert(bt);
//			buildTasksCount++;
//		}
//	}
}

void CBuilderManager::Save(std::ostream& os) const
{
//	std::stringstream tmp;
//	int i = circuit->GetSkirmishAIId();
//	tmp.write(reinterpret_cast<const char*>(&i), sizeof(i));
//	i = 42;
//	tmp.write(reinterpret_cast<const char*>(&i), sizeof(i));
//	os << std::streamsize(1024) << '\n' << tmp.rdbuf();

	/*
	 * Save tasks
	 */
//	int size = buildUpdates.size();
//	os.write(reinterpret_cast<const char*>(&size), sizeof(size));
//	for (IUnitTask* task : buildUpdates) {
//		os << *task;
//	}
//	os.write(reinterpret_cast<const char*>(&buildIterator), sizeof(buildIterator));
}

#ifdef DEBUG_VIS
void CBuilderManager::Log()
{
	for (const std::set<IBuilderTask*>& tasks : buildTasks) {
		for (const IBuilderTask* task : tasks) {
			std::string desc = (task->GetBuildDef() != nullptr) ? task->GetBuildDef()->GetDef()->GetName() : "?";
			desc += utils::int_to_string(static_cast<int>(task->GetBuildType()), "|%i");
			circuit->GetDrawer()->AddPoint(task->GetPosition(), desc.c_str());
		}
	}
}
#endif

} // namespace circuit
