/*
 * MilitaryManager.cpp
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#include "module/MilitaryManager.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "resource/MetalManager.h"
#include "setup/SetupManager.h"
#include "setup/DefenceMatrix.h"
#include "task/NullTask.h"
#include "task/IdleTask.h"
#include "task/RetreatTask.h"
#include "task/builder/DefenceTask.h"
#include "task/fighter/DefendTask.h"
#include "task/fighter/ScoutTask.h"
#include "task/fighter/AttackTask.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#include "json/json.h"

#include "AISCommands.h"
#include "WeaponDef.h"

namespace circuit {

using namespace springai;

CMilitaryManager::CMilitaryManager(CCircuitAI* circuit)
		: IUnitModule(circuit)
		, updateSlice(0)
		, scoutIdx(0)
		, curPowah(.0f)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunParallelTask(CGameTask::emptyTask, std::make_shared<CGameTask>(&CMilitaryManager::Init, this));

	CCircuitDef::Id unitDefId;
	/*
	 * Defence handlers
	 */
	auto defenceFinishedHandler = [this](CCircuitUnit* unit) {
		unit->GetUnit()->Stockpile(UNIT_COMMAND_OPTION_SHIFT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
	};
	auto defenceDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		int frame = this->circuit->GetLastFrame();
		float defCost = unit->GetCircuitDef()->GetCost();
		CDefenceMatrix* defence = this->circuit->GetDefenceMatrix();
		CDefenceMatrix::SDefPoint* point = defence->GetDefPoint(unit->GetPos(frame), defCost);
		if (point != nullptr) {
			point->cost -= defCost;
		}
	};
	const char* defenders[] = {"corllt", "corrl", "corrad", "armsonar", "armdeva", "corhlt", "turrettorp", "corrazor", "armnanotc", "cordoom", "corjamt"/*, "armartic", "corjamt", "armanni", "corbhmth"*/};
	for (const char* name : defenders) {
		unitDefId = circuit->GetCircuitDef(name)->GetId();
		destroyedHandler[unitDefId] = defenceDestroyedHandler;
	}

	/*
	 * Attacker handlers
	 */
	auto attackerCreatedHandler = [this](CCircuitUnit* unit, CCircuitUnit* builder) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			nullTask->AssignTo(unit);
		}
	};
	auto attackerFinishedHandler = [this](CCircuitUnit* unit) {
		if (unit->GetTask() == nullptr) {
			unit->SetManager(this);
			idleTask->AssignTo(unit);
		} else {
			nullTask->RemoveAssignee(unit);
		}

		if (unit->GetCircuitDef()->IsAbleToFly()) {
			unit->GetUnit()->ExecuteCustomCommand(CMD_RETREAT, {2.0f});
		}
	};
	auto attackerIdleHandler = [this](CCircuitUnit* unit) {
		// NOTE: Avoid instant task reassignment, though it may be not relevant for attackers
		if (this->circuit->GetLastFrame() - unit->GetTaskFrame() > FRAMES_PER_SEC) {
			unit->GetTask()->OnUnitIdle(unit);
		}
	};
	auto attackerDamagedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		unit->GetTask()->OnUnitDamaged(unit, attacker);
	};
	auto attackerDestroyedHandler = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
		unit->GetTask()->OnUnitDestroyed(unit, attacker);  // can change task
		unit->GetTask()->RemoveAssignee(unit);  // Remove unit from IdleTask
	};

	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const Json::Value& retreats = root["retreat"];
	const float fighterRet = retreats["_fighter_"].asFloat();

	const CCircuitAI::CircuitDefs& defs = circuit->GetCircuitDefs();
	for (auto& kv : defs) {
		CCircuitDef* cdef = kv.second;
		if (!cdef->IsAttacker() || cdef->GetUnitDef()->IsBuilder()) {
			continue;
		}
		const std::map<std::string, std::string>& customParams = cdef->GetUnitDef()->GetCustomParams();
		auto it = customParams.find("is_drone");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			continue;
		}
		if (cdef->IsMobile()) {
			unitDefId = kv.first;
			createdHandler[unitDefId] = attackerCreatedHandler;
			finishedHandler[unitDefId] = attackerFinishedHandler;
			idleHandler[unitDefId] = attackerIdleHandler;
			damagedHandler[unitDefId] = attackerDamagedHandler;
			destroyedHandler[unitDefId] = attackerDestroyedHandler;

			const char* name = cdef->GetUnitDef()->GetName();
			cdef->SetRetreat(retreats.get(name, fighterRet).asFloat());
		} else {
			WeaponDef* wd = cdef->GetUnitDef()->GetStockpileDef();
			if (wd == nullptr) {
				continue;
			}
			unitDefId = kv.first;
			finishedHandler[unitDefId] = defenceFinishedHandler;
			delete wd;
		}
	}

	/*
	 * raveparty handlers
	 */
	unitDefId = circuit->GetCircuitDef("raveparty")->GetId();
	finishedHandler[unitDefId] = [this](CCircuitUnit* unit) {
		unit->GetUnit()->SetTrajectory(1);
	};

//	/*
//	 * armrectr handlers
//	 */
//	unitDefId = attrib->GetUnitDefByName("armrectr")->GetUnitDefId();
//	idleHandler[unitDefId] = [this](CCircuitUnit* unit) {
//		fighterInfos.erase(unit);
//	};
//	damagedHandler[unitDefId] = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
//		if (attacker != nullptr) {
//			auto search = fighterInfos.find(unit);
//			if (search == fighterInfos.end()) {
//				Unit* u = unit->GetUnit();
//				std::vector<float> params;
//				params.push_back(2.0f);
//				u->ExecuteCustomCommand(CMD_PRIORITY, params);
//
//				const AIFloat3& pos = attacker->GetPos();
//				params.clear();
//				params.push_back(1.0f);  // 1: terraform_type, 1 == level
//				params.push_back(this->circuit->GetTeamId());  // 2: teamId
//				params.push_back(0.0f);  // 3: terraform type - 0 == Wall, else == Area
//				params.push_back(pos.y - 42.0f);  // 4: terraformHeight
//				params.push_back(1.0f);  // 5: number of control points
//				params.push_back(1.0f);  // 6: units count?
//				params.push_back(0.0f);  // 7: volumeSelection?
//				params.push_back(pos.x);  //  8: i + 0 control point x
//				params.push_back(pos.z);  //  9: i + 1 control point z
//				params.push_back(u->GetUnitId());  // 10: i + 2 unitId
//				u->ExecuteCustomCommand(CMD_TERRAFORM_INTERNAL, params);
//
//				fighterInfos[unit].isTerraforming = true;
//			}
//		}
//	};
//	destroyedHandler[unitDefId] = [this](CCircuitUnit* unit, CEnemyUnit* attacker) {
//		fighterInfos.erase(unit);
//	};

	for (const Json::Value& scout : root["scouts"]) {
		CCircuitDef* cdef = circuit->GetCircuitDef(scout.asCString());
		if (cdef == nullptr) {
			continue;
		}
		scoutDefs.insert(cdef);
	}
}

CMilitaryManager::~CMilitaryManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(fighterTasks);
	utils::free_clear(deleteTasks);
}

int CMilitaryManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetCircuitDef()->GetId());
	if (search != createdHandler.end()) {
		search->second(unit, builder);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitFinished(CCircuitUnit* unit)
{
	auto search = finishedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != finishedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitIdle(CCircuitUnit* unit)
{
	auto search = idleHandler.find(unit->GetCircuitDef()->GetId());
	if (search != idleHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	auto search = damagedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != damagedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

int CMilitaryManager::UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	auto search = destroyedHandler.find(unit->GetCircuitDef()->GetId());
	if (search != destroyedHandler.end()) {
		search->second(unit, attacker);
	}

	return 0; //signaling: OK
}

IFighterTask* CMilitaryManager::EnqueueTask(IFighterTask::FightType type)
{
	IFighterTask* task;
	switch (type) {
		default:
		case IFighterTask::FightType::DEFEND: {
//			curPowah += 300.0f;
			task = new CDefendTask(this, curPowah);  // TODO: pass enemy's threat
			break;
		}
		case IFighterTask::FightType::SCOUT: {
			task = new CScoutTask(this);
			break;
		}
		case IFighterTask::FightType::ATTACK: {
			task = new CAttackTask(this);
			break;
		}
	}

	fighterTasks.insert(task);
	return task;
}

void CMilitaryManager::DequeueTask(IFighterTask* task, bool done)
{
	auto it = fighterTasks.find(task);
	if (it != fighterTasks.end()) {
		fighterTasks.erase(it);
		task->Close(done);
		deleteTasks.insert(task);
	}
}

void CMilitaryManager::AssignTask(CCircuitUnit* unit)
{
	IFighterTask* task = nullptr;

//	for (IFighterTask* candidate : fighterTasks) {
//		if (!candidate->CanAssignTo(unit)) {
//			continue;
//		}
//		task = candidate;
//		break;
//	}
//
//	if (task == nullptr) {
		bool isScout = (scoutDefs.find(unit->GetCircuitDef()) != scoutDefs.end());
		IFighterTask::FightType type = isScout ? IFighterTask::FightType::SCOUT : IFighterTask::FightType::DEFEND;
		task = EnqueueTask(type);
//	}

	task->AssignTo(unit);
}

void CMilitaryManager::AbortTask(IUnitTask* task)
{
	DequeueTask(static_cast<IFighterTask*>(task), false);
}

void CMilitaryManager::DoneTask(IUnitTask* task)
{
	DequeueTask(static_cast<IFighterTask*>(task), true);
}

void CMilitaryManager::FallbackTask(CCircuitUnit* unit)
{
}

void CMilitaryManager::MakeDefence(const AIFloat3& pos)
{
	int index = circuit->GetMetalManager()->FindNearestCluster(pos);
	if (index < 0) {
		return;
	}
	CCircuitDef* defDef;
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	float maxCost = MIN_BUILD_SEC * std::min(economyManager->GetAvgMetalIncome(), economyManager->GetAvgEnergyIncome());
	CDefenceMatrix* defence = circuit->GetDefenceMatrix();
	CDefenceMatrix::SDefPoint* closestPoint = nullptr;
	float minDist = std::numeric_limits<float>::max();
	for (CDefenceMatrix::SDefPoint& defPoint : defence->GetDefPoints(index)) {
		if (defPoint.cost < maxCost) {
			float dist = defPoint.position.SqDistance2D(pos);
			if ((closestPoint == nullptr) || (dist < minDist)) {
				closestPoint = &defPoint;
				minDist = dist;
			}
		}
	}
	if (closestPoint == nullptr) {
		return;
	}
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	float totalCost = .0f;
	IBuilderTask* parentTask = nullptr;
	bool isWater = circuit->GetTerrainManager()->IsWaterSector(pos);
	std::array<const char*, 9> landDefenders = {"corllt", "corrl", "corrad", "corrl", "corhlt", "corrazor", "armnanotc", "cordoom", "corjamt"/*, "armanni", "corbhmth"*/};
// FIXME: DEBUG
//	std::array<const char*, 9> landDefenders = {"armdeva", "corllt", "corrad", "corrl", "corrazor", "armnanotc", "corrl", "corrl", "corrl"};
// FIXME: DEBUG
	std::array<const char*, 9> waterDefenders = {"turrettorp", "armsonar", "corllt", "corrad", "corrazor", "armnanotc", "turrettorp", "corhlt", "turrettorp"};
	std::array<const char*, 9>& defenders = isWater ? waterDefenders : landDefenders;
	for (const char* name : defenders) {
		defDef = circuit->GetCircuitDef(name);
		float defCost = defDef->GetCost();
		totalCost += defCost;
		if (totalCost <= closestPoint->cost) {
			continue;
		}
		if (totalCost < maxCost) {
			closestPoint->cost += defCost;
			bool isFirst = (parentTask == nullptr);
// FIXME: DEBUG
//			if (std::string("armdeva") == name) {
//				const float terraCost = 1.0f;
//				closestPoint->cost += terraCost;
//				IBuilderTask* task = builderManager->EnqueueTerraform(IBuilderTask::Priority::HIGH, nullptr, closestPoint->position, terraCost, isFirst);
//				if (parentTask != nullptr) {
//					parentTask->SetNextTask(task);
//				}
//				parentTask = task;
//				isFirst = false;
//			}
// FIXME: DEBUG
			IBuilderTask::Priority priority = isFirst ? IBuilderTask::Priority::HIGH : IBuilderTask::Priority::NORMAL;
			IBuilderTask* task = builderManager->EnqueueTask(priority, defDef, closestPoint->position,
															 IBuilderTask::BuildType::DEFENCE, true, isFirst);
			if (parentTask != nullptr) {
				parentTask->SetNextTask(task);
			}
			parentTask = task;
		} else {
			// TODO: Auto-sort defenders by cost OR remove break?
			break;
		}
	}
}

void CMilitaryManager::AbortDefence(CBDefenceTask* task)
{
	float defCost = task->GetBuildDef()->GetCost();
	CDefenceMatrix::SDefPoint* point = circuit->GetDefenceMatrix()->GetDefPoint(task->GetPosition(), defCost);
	if (point != nullptr) {
		if ((task->GetTarget() == nullptr) && (point->cost >= defCost)) {
			point->cost -= defCost;
		}
		IBuilderTask* next = task->GetNextTask();
		while (next != nullptr) {
			if (next->GetBuildDef() != nullptr) {
				defCost = next->GetBuildDef()->GetCost();
			} else{
				defCost = next->GetCost();
			}
			if (point->cost >= defCost) {
				point->cost -= defCost;
			}
			next = next->GetNextTask();
		}
	}

}

AIFloat3 CMilitaryManager::GetScoutPosition(CCircuitUnit* unit)
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	STerrainMapArea* area = unit->GetArea();
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Metals& spots = metalManager->GetSpots();
	decltype(scoutIdx) prevIdx = scoutIdx;
	while (scoutIdx < scoutPath.size()) {
		int index = scoutPath[scoutIdx++];
		if (!metalManager->IsMexInFinished(index) && terrainManager->CanMoveToPos(area, spots[index].position)) {
			return spots[index].position;
		}
	}
	scoutIdx = 0;
	while (scoutIdx < prevIdx) {
		int index = scoutPath[scoutIdx++];
		if (!metalManager->IsMexInFinished(index) && terrainManager->CanMoveToPos(area, spots[index].position)) {
			return spots[index].position;
		}
	}
//	++scoutIdx %= scoutPath.size();
	return -RgtVector;
}

void CMilitaryManager::Init()
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Metals& spots = metalManager->GetSpots();

	scoutPath.reserve(spots.size());
	for (unsigned i = 0; i < spots.size(); ++i) {
		scoutPath.push_back(i);
	}
	const AIFloat3& pos = circuit->GetSetupManager()->GetStartPos();
	auto compare = [&pos, &spots](int a, int b) {
		return pos.SqDistance2D(spots[a].position) > pos.SqDistance2D(spots[b].position);
	};
	std::sort(scoutPath.begin(), scoutPath.end(), compare);

	CScheduler* scheduler = circuit->GetScheduler().get();
	const int interval = 4;
	const int offset = circuit->GetSkirmishAIId() % interval;
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::UpdateIdle, this), interval, offset + 0);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::UpdateFight, this), interval / 2, offset + 1);
	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMilitaryManager::UpdateRetreat, this), interval, offset + 2);
}

void CMilitaryManager::UpdateIdle()
{
	idleTask->Update();
}

void CMilitaryManager::UpdateRetreat()
{
	retreatTask->Update();
}

void CMilitaryManager::UpdateFight()
{
	if (!deleteTasks.empty()) {
		for (auto task : deleteTasks) {
			updateTasks.erase(task);
			delete task;
		}
		deleteTasks.clear();
	}

	auto it = updateTasks.begin();
	unsigned int i = 0;
	while (it != updateTasks.end()) {
		(*it)->Update();

		it = updateTasks.erase(it);
		if (++i >= updateSlice) {
			break;
		}
	}

	if (updateTasks.empty()) {
		updateTasks = fighterTasks;
		updateSlice = updateTasks.size() / TEAM_SLOWUPDATE_RATE;
	}
}

} // namespace circuit
