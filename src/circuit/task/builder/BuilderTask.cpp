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
				savedIncome(manager->GetCircuit()->GetEconomyManager()->GetAvgMetalIncome())
{
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

	// Alter/randomize position
	AIFloat3 offset((float)rand() / RAND_MAX - 0.5f, 0.0f, (float)rand() / RAND_MAX - 0.5f);
	AIFloat3 pos = position + offset * SQUARE_SIZE * 16;

	CTerrainManager::TerrainPredicate predicate = [terrain, unit](const AIFloat3& p) {
		return terrain->CanBuildAt(unit, p);
	};
	float searchRadius = 200.0f * SQUARE_SIZE;
	facing = FindFacing(buildDef, pos);
	buildPos = terrain->FindBuildSite(buildDef, pos, searchRadius, facing, predicate);
	// TODO: if (buildPos == -RgtVector) then assign/select other StartPos?

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

	// FIXME: Replace const 1000.0f with build time?
	if (cost < 1000.0f) {
		return;
	}
	float currentIncome = circuit->GetEconomyManager()->GetAvgMetalIncome();
	if (currentIncome < savedIncome * 0.6) {
		manager->AbortTask(this);
		if (target != nullptr) {
			for (auto haven : circuit->GetFactoryManager()->GetHavensAt(buildPos)) {
				IBuilderTask* task = static_cast<IBuilderTask*>(haven->GetTask());
				if (task->GetTarget() == target) {
					CFactoryManager* factoryManager = static_cast<CFactoryManager*>(haven->GetManager());
					factoryManager->AbortTask(task);
				}
			}
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
	if (u->GetHealth() >= u->GetMaxHealth() * 0.8) {
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

UnitDef* IBuilderTask::GetBuildDef() const
{
	return buildDef;
}

IBuilderTask::BuildType IBuilderTask::GetBuildType() const
{
	return buildType;
}

float IBuilderTask::GetBuildPower() const
{
	return buildPower;
}

float IBuilderTask::GetCost() const
{
	return cost;
}

int IBuilderTask::GetTimeout() const
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
	SetBuildPos((unit != nullptr) ? unit->GetUnit()->GetPos() : -RgtVector);
}

CCircuitUnit* IBuilderTask::GetTarget() const
{
	return target;
}

bool IBuilderTask::IsStructure() const
{
	return (buildType < IBuilderTask::BuildType::MEX);
}

void IBuilderTask::SetFacing(int value)
{
	facing = value;
}

int IBuilderTask::GetFacing() const
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
