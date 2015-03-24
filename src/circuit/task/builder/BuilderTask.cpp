/*
 * BuilderTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "task/builder/BuilderTask.h"
#include "task/RetreatTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
#include "unit/action/UnitAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "UnitDef.h"
#include "Unit.h"
#include "Map.h"

namespace circuit {

using namespace springai;

IBuilderTask::IBuilderTask(ITaskManager* mgr, Priority priority,
		UnitDef* buildDef, const AIFloat3& position,
		BuildType type, float cost, int timeout) :
				IUnitTask(mgr, priority, Type::BUILDER),
				buildDef(buildDef),
				position(position),
				buildType(type),
				cost(cost),
				timeout(timeout),
				target(nullptr),
				buildPos(-RgtVector),
				buildPower(.0f),
				facing(UNIT_COMMAND_BUILD_NO_FACING),
				savedIncome(.0f)
{
	// TODO: Move into IBuilderTask::Execute?
	if (IsStructure()) {
		// Alter/randomize position
		AIFloat3 offset((float)rand() / RAND_MAX - 0.5f, 0.0f, (float)rand() / RAND_MAX - 0.5f);
		this->position += offset * SQUARE_SIZE * 16;
	}
}

IBuilderTask::~IBuilderTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool IBuilderTask::CanAssignTo(CCircuitUnit* unit)
{
	return (((target != nullptr) || unit->GetCircuitDef()->CanBuild(buildDef)) && (cost > buildPower * MIN_BUILD_SEC));
}

void IBuilderTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	buildPower += unit->GetDef()->GetBuildSpeed();
}

void IBuilderTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);

	buildPower -= unit->GetDef()->GetBuildSpeed();
}

void IBuilderTask::Execute(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();

	std::vector<float> params;
	params.push_back(static_cast<float>(priority));
	u->ExecuteCustomCommand(CMD_PRIORITY, params);

	if (target != nullptr) {
		Unit* tu = target->GetUnit();
		u->Build(target->GetDef(), tu->GetPos(), tu->GetBuildingFacing(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
		return;
	}
	CCircuitAI* circuit = manager->GetCircuit();
	if (buildPos != -RgtVector) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildDef, buildPos, facing)) {
			u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			return;
		} else {
			circuit->GetTerrainManager()->RemoveBlocker(buildDef, buildPos, facing);
		}
	}

	CTerrainManager* terrain = circuit->GetTerrainManager();

	// FIXME: Replace const 999.0f with build time?
	if (circuit->IsAllyAware() && (cost > 999.0f)) {
		circuit->UpdateAllyUnits();
		// TODO: Use OOAICallback::GetFriendlyUnitsIn()?
		const std::map<int, CCircuitUnit*>& allies = circuit->GetAllyUnits();
		float sqDist = cost * cost;
		for (auto& kv : allies) {
			CCircuitUnit* alu = kv.second;
			Unit* au = alu->GetUnit();
			if ((alu->GetDef() == buildDef) && au->IsBeingBuilt()) {
				const AIFloat3& pos = au->GetPos();
				if ((position.SqDistance2D(pos) < sqDist) && terrain->CanBuildAt(unit, pos)) {
					u->Build(buildDef, pos, au->GetBuildingFacing(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
					return;
				}
			}
		}
	}

	CTerrainManager::TerrainPredicate predicate = [terrain, unit](const AIFloat3& p) {
		return terrain->CanBuildAt(unit, p);
	};
	float searchRadius = 200.0f * SQUARE_SIZE;
	facing = FindFacing(buildDef, position);
	buildPos = terrain->FindBuildSite(buildDef, position, searchRadius, facing, predicate);
	if (buildPos == -RgtVector) {
		const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
		const CMetalData::MetalIndices indices = circuit->GetMetalManager()->FindNearestClusters(position, 3);
		for (const int idx : indices) {
			facing = FindFacing(buildDef, clusters[idx].geoCentr);
			buildPos = terrain->FindBuildSite(buildDef, clusters[idx].geoCentr, searchRadius, facing, predicate);
			if (buildPos != -RgtVector) {
				break;
			}
		}
	}

	if (buildPos != -RgtVector) {
		circuit->GetTerrainManager()->AddBlocker(buildDef, buildPos, facing);
		u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

void IBuilderTask::Update()
{
	// tODO: Analyze nearby situation, maybe cancel this task
	CCircuitAI* circuit = manager->GetCircuit();
	for (auto unit : units) {
		IUnitAction* action = static_cast<IUnitAction*>(unit->Begin());
		if (action->GetType() == IUnitAction::Type::PRE_BUILD) {
			Unit* u = unit->GetUnit();
			const AIFloat3& vel = u->GetVel();
			Resource* metal = circuit->GetEconomyManager()->GetMetalRes();
			if ((vel == ZeroVector) && (u->GetResourceUse(metal) <= 0)) {
				// TODO: Something is on build site, get standing units in radius and push them.
			}
		}
	}

	// FIXME: Replace const 999.0f with build time?
	if ((cost > 999.0f) && (savedIncome > .0f)) {
		CCircuitAI* circuit = manager->GetCircuit();
		float currentIncome = circuit->GetEconomyManager()->GetAvgMetalIncome();
		if (currentIncome < savedIncome * 0.6) {
			if ((target != nullptr) && (target->GetUnit()->GetHealth() < target->GetUnit()->GetMaxHealth() * 0.2)) {
				/*
				 * FIXME: The logic is broken here. It creates Reclaim task, but its based on timeout
				 * otherwise it won't be removed from task queue. And nearby patrollers or ally units
				 * can still build the target.
				 * Maybe just AbortTask and on RepairTask check same conditions.
				 */
				CBuilderManager* builderManager = circuit->GetBuilderManager();
				IBuilderTask* task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, buildPos,
																 IBuilderTask::BuildType::RECLAIM, FRAMES_PER_SEC * MAX_BUILD_SEC);
				task->SetTarget(target);
				std::set<CCircuitUnit*> us = units;
				for (auto unit : us) {
					manager->AssignTask(unit, task);
				}

				CTerrainManager* terrain = circuit->GetTerrainManager();
				auto reclaim = [this, terrain](Unit* u) {
					u->ReclaimUnit(target->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
					AIFloat3 toPos = u->GetPos();
					toPos.x += (toPos.x > terrain->GetTerrainWidth() / 2) ? -SQUARE_SIZE : SQUARE_SIZE;
					toPos.z += (toPos.z > terrain->GetTerrainHeight() / 2) ? -SQUARE_SIZE : SQUARE_SIZE;
					u->PatrolTo(toPos, UNIT_COMMAND_OPTION_SHIFT_KEY);
				};

				std::vector<CCircuitUnit*> havens = circuit->GetFactoryManager()->GetHavensAt(buildPos);
				for (auto haven : havens) {
					reclaim(haven->GetUnit());
				}

//				std::vector<CCircuitUnit*> patrollers = builderManager->GetPatrollersAt(buildPos);
//				for (auto patrol : patrollers) {
//					reclaim(patrol->GetUnit());
//				}
			}
			manager->AbortTask(this);
		}
	}
}

void IBuilderTask::Cancel()
{
	if (target == nullptr) {
		manager->GetCircuit()->GetTerrainManager()->RemoveBlocker(buildDef, buildPos, facing);
	}
}

void IBuilderTask::OnUnitIdle(CCircuitUnit* unit)
{
	RemoveAssignee(unit);
}

void IBuilderTask::OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	Unit* u = unit->GetUnit();
	// TODO: floating retreat coefficient
	if (u->GetHealth() >= u->GetMaxHealth() * 0.9) {
		return;
	}

	if (target == nullptr) {
		manager->AbortTask(this);
	}

	manager->AssignTask(unit, manager->GetRetreatTask());
}

void IBuilderTask::OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	manager->AbortTask(this);
}

const AIFloat3& IBuilderTask::GetTaskPos() const
{
	return position;
}

UnitDef* IBuilderTask::GetBuildDef()
{
	return buildDef;
}

IBuilderTask::BuildType IBuilderTask::GetBuildType()
{
	return buildType;
}

float IBuilderTask::GetBuildPower()
{
	return buildPower;
}

float IBuilderTask::GetCost()
{
	return cost;
}

int IBuilderTask::GetTimeout()
{
	return timeout;
}

void IBuilderTask::SetBuildPos(const AIFloat3& pos)
{
	buildPos = pos;
}

const AIFloat3& IBuilderTask::GetBuildPos() const
{
	return buildPos;
}

const AIFloat3& IBuilderTask::GetPosition() const
{
	return (buildPos != -RgtVector) ? buildPos : position;
}

void IBuilderTask::SetTarget(CCircuitUnit* unit)
{
	target = unit;
	if (unit != nullptr) {
		SetBuildPos(unit->GetUnit()->GetPos());
		savedIncome = manager->GetCircuit()->GetEconomyManager()->GetAvgMetalIncome();
	} else {
		SetBuildPos(-RgtVector);
		savedIncome = .0f;
	}
}

CCircuitUnit* IBuilderTask::GetTarget()
{
	return target;
}

bool IBuilderTask::IsStructure()
{
	return (buildType < IBuilderTask::BuildType::MEX);
}

void IBuilderTask::SetFacing(int value)
{
	facing = value;
}

int IBuilderTask::GetFacing()
{
	return facing;
}

int IBuilderTask::FindFacing(springai::UnitDef* buildDef, const springai::AIFloat3& position)
{
	int facing = UNIT_COMMAND_BUILD_NO_FACING;
	CTerrainManager* terrain = manager->GetCircuit()->GetTerrainManager();
	float terWidth = terrain->GetTerrainWidth();
	float terHeight = terrain->GetTerrainHeight();
	if (math::fabs(terWidth - 2 * position.x) > math::fabs(terHeight - 2 * position.z)) {
		facing = (2 * position.x > terWidth) ? UNIT_FACING_WEST : UNIT_FACING_EAST;
	} else {
		facing = (2 * position.z > terHeight) ? UNIT_FACING_NORTH : UNIT_FACING_SOUTH;
	}
	return facing;
}

} // namespace circuit
