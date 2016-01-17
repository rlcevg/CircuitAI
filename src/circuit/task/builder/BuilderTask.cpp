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
#include "resource/MetalManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "unit/action/UnitAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

IBuilderTask::IBuilderTask(ITaskManager* mgr, Priority priority,
		CCircuitDef* buildDef, const AIFloat3& position,
		BuildType type, float cost, bool isShake, int timeout)
				: IUnitTask(mgr, priority, Type::BUILDER, timeout)
				, position(position)
				, isShake(isShake)
				, buildDef(buildDef)
				, buildType(type)
				, buildPower(.0f)
				, cost(cost)
				, target(nullptr)
				, buildPos(-RgtVector)
				, facing(UNIT_COMMAND_BUILD_NO_FACING)
				, nextTask(nullptr)
				, buildFails(0)
{
	savedIncome = manager->GetCircuit()->GetEconomyManager()->GetAvgMetalIncome();
}

IBuilderTask::IBuilderTask(ITaskManager* mgr, Priority priority,
		CCircuitDef* buildDef, const AIFloat3& position,
		float cost, int timeout)
				: IUnitTask(mgr, priority, Type::FACTORY, timeout)
				, position(position)
				, isShake(false)
				, buildDef(buildDef)
				, buildType(BuildType::RECRUIT)
				, buildPower(.0f)
				, cost(cost)
				, target(nullptr)
				, buildPos(-RgtVector)
				, facing(UNIT_COMMAND_BUILD_NO_FACING)
				, nextTask(nullptr)
				, buildFails(0)
{
	savedIncome = manager->GetCircuit()->GetEconomyManager()->GetAvgMetalIncome();
}

IBuilderTask::~IBuilderTask()
{
}

bool IBuilderTask::CanAssignTo(CCircuitUnit* unit)
{
	return (((target != nullptr) || unit->GetCircuitDef()->CanBuild(buildDef)) && (cost > buildPower * MIN_BUILD_SEC));
}

void IBuilderTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	ShowAssignee(unit);
	if (position == -RgtVector) {
		position = unit->GetPos(manager->GetCircuit()->GetLastFrame());
	}
}

void IBuilderTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);

	HideAssignee(unit);
}

void IBuilderTask::Execute(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	u->ExecuteCustomCommand(CMD_PRIORITY, {ClampPriority()});

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	if (target != nullptr) {
		int facing = target->GetUnit()->GetBuildingFacing();
		u->Build(target->GetCircuitDef()->GetUnitDef(), buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
		return;
	}
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	UnitDef* buildUDef = buildDef->GetUnitDef();
	if (buildPos != -RgtVector) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
			u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
			return;
		} else {
			terrainManager->RemoveBlocker(buildDef, buildPos, facing);
			// FIXME: If enemy blocked position then reset will have no effect
//			terrainManager->ResetBuildFrame();
		}
	}

	circuit->GetThreatMap()->SetThreatType(unit);
	// FIXME: Replace const 999.0f with build time?
	if (circuit->IsAllyAware() && (cost > 999.0f)) {
		circuit->UpdateFriendlyUnits();
		auto friendlies = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(position, cost));
		for (Unit* au : friendlies) {
			CCircuitUnit* alu = circuit->GetFriendlyUnit(au);
			if (alu == nullptr) {
				continue;
			}
			if ((*alu->GetCircuitDef() == *buildDef) && au->IsBeingBuilt()) {
				const AIFloat3& pos = alu->GetPos(frame);
				if (terrainManager->CanBuildAt(unit, pos)) {
					u->Build(buildUDef, pos, au->GetBuildingFacing(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
					utils::free_clear(friendlies);
					return;
				}
			}
		}
		utils::free_clear(friendlies);
	}

	// Alter/randomize position
	AIFloat3 pos;
	if (isShake) {
		AIFloat3 offset((float)rand() / RAND_MAX - 0.5f, 0.0f, (float)rand() / RAND_MAX - 0.5f);
		pos = position + offset * SQUARE_SIZE * 16;
	} else {
		pos = position;
	}

	const float searchRadius = 200.0f * SQUARE_SIZE;
	FindBuildSite(unit, pos, searchRadius);

	if (buildPos != -RgtVector) {
		terrainManager->AddBlocker(buildDef, buildPos, facing);
		u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
	} else {
		// TODO: Select new proper BasePos, like near metal cluster.
		int terWidth = terrainManager->GetTerrainWidth();
		int terHeight = terrainManager->GetTerrainHeight();
		float x = terWidth / 4 + rand() % (int)(terWidth / 2 + 1);
		float z = terHeight / 4 + rand() % (int)(terHeight / 2 + 1);
		AIFloat3 pos(x, circuit->GetMap()->GetElevationAt(x, z), z);
		circuit->GetSetupManager()->SetBasePos(pos);

		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

void IBuilderTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
//	for (auto unit : units) {
//		IUnitAction* action = static_cast<IUnitAction*>(unit->Begin());
//		if (action->GetType() == IUnitAction::Type::PRE_BUILD) {
//			Unit* u = unit->GetUnit();
//			const AIFloat3& vel = u->GetVel();
//			Resource* metal = circuit->GetEconomyManager()->GetMetalRes();
//			if ((vel == ZeroVector) && (u->GetResourceUse(metal) <= 0)) {
//				// TODO: Something is on build site, get standing units in radius and push them.
//			}
//		}
//	}

	// FIXME: Replace const 1000.0f with build time?
	if ((cost > 1000.0f) && (circuit->GetEconomyManager()->GetAvgMetalIncome() < savedIncome * 0.6f)) {
		manager->AbortTask(this);
		return;
	}

	// Reassign task if required
	if (units.empty()) {
		return;
	}
	CCircuitUnit* unit = *units.begin();
	HideAssignee(unit);
	IUnitTask* task = manager->GetTask(unit);
	ShowAssignee(unit);
	if ((task != nullptr) && (task != this)) {
		manager->AssignTask(unit, task);
	}
}

void IBuilderTask::Finish()
{
	// FIXME: Replace const 1000.0f with build time?
//	if ((cost > 1000.0f) && (buildDef != nullptr) && !buildDef->IsAttacker()) {
//		manager->GetCircuit()->GetBuilderManager()->EnqueueTerraform(IBuilderTask::Priority::HIGH, target);
//	}

	// Advance queue
	if (nextTask != nullptr) {
		manager->GetCircuit()->GetBuilderManager()->ActivateTask(nextTask);
		nextTask = nullptr;
	}
}

void IBuilderTask::Cancel()
{
	if ((target == nullptr) && (buildPos != -RgtVector)) {
		manager->GetCircuit()->GetTerrainManager()->RemoveBlocker(buildDef, buildPos, facing);
	}

	// Destroy queue
	IBuilderTask* next = nextTask;
	while (next != nullptr) {
		IBuilderTask* nextNext = next->nextTask;
		delete next;
		next = nextNext;
	}
}

void IBuilderTask::OnUnitIdle(CCircuitUnit* unit)
{
	if (++buildFails <= 2) {  // Workaround due to engine's ability randomly disregard orders
		Execute(unit);
	} else if (buildFails <= TASK_RETRIES) {
		RemoveAssignee(unit);
	} else {
		manager->AbortTask(this);
		manager->GetCircuit()->GetTerrainManager()->AddBlocker(buildDef, buildPos, facing);  // FIXME: Remove blocker on timer? Or when air con appears
	}
}

void IBuilderTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	Unit* u = unit->GetUnit();
	if (u->GetHealth() >= u->GetMaxHealth() * unit->GetCircuitDef()->GetRetreat()) {
		return;
	}

	if (target == nullptr) {
		manager->AbortTask(this);
	}

	CRetreatTask* task = manager->GetCircuit()->GetBuilderManager()->EnqueueRetreat();
	manager->AssignTask(unit, task);
}

void IBuilderTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	// NOTE: AbortTask usually do not call RemoveAssignee for each unit
	if (((target == nullptr) || units.empty()) && !unit->IsMorphing()) {
		manager->AbortTask(this);
	} else {
		RemoveAssignee(unit);
	}
}

void IBuilderTask::SetTarget(CCircuitUnit* unit)
{
	target = unit;
	SetBuildPos((unit != nullptr) ? unit->GetPos(manager->GetCircuit()->GetLastFrame()) : AIFloat3(-RgtVector));
}

void IBuilderTask::UpdateTarget(CCircuitUnit* unit)
{
	// NOTE: Can't use SetTarget because unit->GetPos() may be different from buildPos
	target = unit;

	int frame = manager->GetCircuit()->GetLastFrame() + FRAMES_PER_SEC * 60;
	for (CCircuitUnit* ass : units) {
		ass->GetUnit()->Build(buildDef->GetUnitDef(), buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame);
	}
}

bool IBuilderTask::IsEqualBuildPos(CCircuitUnit* unit) const
{
	AIFloat3 pos = unit->GetPos(manager->GetCircuit()->GetLastFrame());
	const AIFloat3& offset = unit->GetCircuitDef()->GetMidPosOffset();
	int facing = unit->GetUnit()->GetBuildingFacing();
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH: {
			pos.x -= offset.x;
			pos.z -= offset.z;
		} break;
		case UNIT_FACING_EAST: {
			pos.x -= offset.z;
			pos.z += offset.x;
		} break;
		case UNIT_FACING_NORTH: {
			pos.x += offset.x;
			pos.z += offset.z;
		} break;
		case UNIT_FACING_WEST: {
			pos.x += offset.z;
			pos.z -= offset.x;
		} break;
	}
	// NOTE: Unit's position is affected by collisionVolumeOffsets, and there is no way to retrieve it.
	//       Hence absurdly large error slack, @see factoryship.lua
	return utils::is_equal_pos(pos, buildPos, SQUARE_SIZE * 2);
}

void IBuilderTask::HideAssignee(CCircuitUnit* unit)
{
	buildPower -= unit->GetCircuitDef()->GetBuildSpeed();
}

void IBuilderTask::ShowAssignee(CCircuitUnit* unit)
{
	buildPower += unit->GetCircuitDef()->GetBuildSpeed();
}

void IBuilderTask::FindBuildSite(CCircuitUnit* builder, const AIFloat3& pos, float searchRadius)
{
	CTerrainManager* terrainManager = manager->GetCircuit()->GetTerrainManager();

//	facing = UNIT_COMMAND_BUILD_NO_FACING;
	float terWidth = terrainManager->GetTerrainWidth();
	float terHeight = terrainManager->GetTerrainHeight();
	if (math::fabs(terWidth - 2 * pos.x) > math::fabs(terHeight - 2 * pos.z)) {
		facing = (2 * pos.x > terWidth) ? UNIT_FACING_WEST : UNIT_FACING_EAST;
	} else {
		facing = (2 * pos.z > terHeight) ? UNIT_FACING_NORTH : UNIT_FACING_SOUTH;
	}

	CTerrainManager::TerrainPredicate predicate = [terrainManager, builder](const AIFloat3& p) {
		return terrainManager->CanBuildAt(builder, p);
	};
	buildPos = terrainManager->FindBuildSite(buildDef, pos, searchRadius, facing, predicate);
}

} // namespace circuit
