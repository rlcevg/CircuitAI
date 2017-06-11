/*
 * BuilderTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "task/builder/BuilderTask.h"
#include "task/builder/BuildChain.h"
#include "task/RetreatTask.h"
#include "module/EconomyManager.h"
#include "module/BuilderManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "resource/EnergyGrid.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "unit/action/DGunAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

IBuilderTask::BuildName IBuilderTask::buildNames = {
	{"factory", IBuilderTask::BuildType::FACTORY},
	{"nano",    IBuilderTask::BuildType::NANO},
	{"store",   IBuilderTask::BuildType::STORE},
	{"pylon",   IBuilderTask::BuildType::PYLON},
	{"energy",  IBuilderTask::BuildType::ENERGY},
	{"defence", IBuilderTask::BuildType::DEFENCE},
	{"bunker",  IBuilderTask::BuildType::BUNKER},
	{"big_gun", IBuilderTask::BuildType::BIG_GUN},
	{"radar",   IBuilderTask::BuildType::RADAR},
	{"sonar",   IBuilderTask::BuildType::SONAR},
	{"mex",     IBuilderTask::BuildType::MEX},
	{"repair",  IBuilderTask::BuildType::REPAIR},
};

IBuilderTask::IBuilderTask(ITaskManager* mgr, Priority priority,
						   CCircuitDef* buildDef, const AIFloat3& position,
						   Type type, BuildType buildType, float cost, float shake, int timeout)
		: IUnitTask(mgr, priority, type, timeout)
		, position(position)
		, shake(shake)
		, buildDef(buildDef)
		, buildType(buildType)
		, buildPower(.0f)
		, cost(cost)
		, target(nullptr)
		, buildPos(-RgtVector)
		, facing(UNIT_COMMAND_BUILD_NO_FACING)
		, nextTask(nullptr)
		, buildFails(0)
		, unitIt(units.end())
{
	savedIncome = manager->GetCircuit()->GetEconomyManager()->GetAvgMetalIncome();
}

IBuilderTask::~IBuilderTask()
{
	delete nextTask;
}

bool IBuilderTask::CanAssignTo(CCircuitUnit* unit) const
{
	return ((target != nullptr) || unit->GetCircuitDef()->CanBuild(buildDef)) && (cost > buildPower * MIN_BUILD_SEC);
}

void IBuilderTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	ShowAssignee(unit);
	if (!utils::is_valid(position)) {
		position = unit->GetPos(manager->GetCircuit()->GetLastFrame());
	}

	if (unit->HasDGun()) {
		CDGunAction* act = new CDGunAction(unit, unit->GetDGunRange());
		unit->PushBack(act);
	}
}

void IBuilderTask::RemoveAssignee(CCircuitUnit* unit)
{
	if ((units.find(unit) == unitIt) && (unitIt != units.end())) {
		++unitIt;
	}
	IUnitTask::RemoveAssignee(unit);

	HideAssignee(unit);
}

void IBuilderTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	Unit* u = unit->GetUnit();
	TRY_UNIT(circuit, unit,
		u->ExecuteCustomCommand(CMD_PRIORITY, {ClampPriority()});
	)

	int frame = circuit->GetLastFrame();
	if (target != nullptr) {
		int facing = target->GetUnit()->GetBuildingFacing();
		TRY_UNIT(circuit, unit,
			u->Build(target->GetCircuitDef()->GetUnitDef(), buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
		)
		return;
	}
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	UnitDef* buildUDef = buildDef->GetUnitDef();
	if (utils::is_valid(buildPos)) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
			TRY_UNIT(circuit, unit,
				u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
			)
			return;
		} else {
			terrainManager->DelBlocker(buildDef, buildPos, facing);
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
					TRY_UNIT(circuit, unit,
						u->Build(buildUDef, pos, au->GetBuildingFacing(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
					)
					utils::free_clear(friendlies);
					return;
				}
			}
		}
		utils::free_clear(friendlies);
	}

	// Alter/randomize position
	AIFloat3 pos = (shake > .0f) ? utils::get_near_pos(position, shake) : position;

	const float searchRadius = 200.0f * SQUARE_SIZE;
	FindBuildSite(unit, pos, searchRadius);

	if (utils::is_valid(buildPos)) {
		terrainManager->AddBlocker(buildDef, buildPos, facing);
		TRY_UNIT(circuit, unit,
			u->Build(buildUDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
		)
	} else {
		// TODO: Select new proper BasePos, like near metal cluster.
		int terWidth = terrainManager->GetTerrainWidth();
		int terHeight = terrainManager->GetTerrainHeight();
		float x = terWidth / 4 + rand() % (int)(terWidth / 2);
		float z = terHeight / 4 + rand() % (int)(terHeight / 2);
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
	CEconomyManager* em = circuit->GetEconomyManager();
	if ((cost > 1000.0f) &&
		(target == nullptr) &&
		(em->GetAvgMetalIncome() < savedIncome * 0.6f) &&
		(em->GetAvgMetalIncome() * 2.0f < em->GetMetalPull()))
	{
		manager->AbortTask(this);
		return;
	}

	// Reassign task if required
	if (units.empty()) {
		return;
	}
	if (unitIt == units.end()) {
		unitIt = units.begin();
	}
	CCircuitUnit* unit = *unitIt;
	++unitIt;

	const float sqDist = unit->GetPos(circuit->GetLastFrame()).SqDistance2D(GetPosition());
	if (sqDist <= SQUARE(unit->GetCircuitDef()->GetBuildDistance())) {
		return;
	}
	HideAssignee(unit);
	IBuilderTask* task = static_cast<IBuilderTask*>(manager->MakeTask(unit));
	ShowAssignee(unit);
	if ((task != nullptr) && (task->GetBuildType() != buildType)) {
		manager->AssignTask(unit, task);
	}
}

void IBuilderTask::Close(bool done)
{
	IUnitTask::Close(done);

	if ((buildDef != nullptr) && !manager->GetCircuit()->GetEconomyManager()->IsIgnorePull(this)) {
		manager->DelMetalPull(buildPower);
	}
}

void IBuilderTask::Finish()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	if (buildDef != nullptr) {
		SBuildChain* chain = builderManager->GetBuildChain(buildType, buildDef);
		if (chain != nullptr) {
			ExecuteChain(chain);
		}

		const int buildDelay = circuit->GetEconomyManager()->GetBuildDelay();
		if (buildDelay > 0) {
			IUnitTask* task = builderManager->EnqueueWait(buildDelay);
			decltype(units) tmpUnits = units;
			for (CCircuitUnit* unit : tmpUnits) {
				manager->AssignTask(unit, task);
			}
		}
	}

	// Advance queue
	if (nextTask != nullptr) {
		builderManager->ActivateTask(nextTask);
		nextTask = nullptr;
	}
}

void IBuilderTask::Cancel()
{
	if ((target == nullptr) && utils::is_valid(buildPos)) {
		manager->GetCircuit()->GetTerrainManager()->DelBlocker(buildDef, buildPos, facing);
	}

	// Destructor will take care of the nextTask queue
}

void IBuilderTask::OnUnitIdle(CCircuitUnit* unit)
{
	if (++buildFails <= 2) {  // Workaround due to engine's ability randomly disregard orders
		Execute(unit);
	} else if (buildFails <= TASK_RETRIES) {
		RemoveAssignee(unit);
	} else if (target == nullptr) {
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
	RemoveAssignee(unit);
	// NOTE: AbortTask usually do not call RemoveAssignee for each unit
	if (((target == nullptr) || units.empty()) && !unit->IsMorphing()) {
		manager->AbortTask(this);
	}
}

void IBuilderTask::Activate()
{
	lastTouched = manager->GetCircuit()->GetLastFrame();
}

void IBuilderTask::Deactivate()
{
	lastTouched = -1;
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

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame() + FRAMES_PER_SEC * 60;
	for (CCircuitUnit* ass : units) {
		TRY_UNIT(circuit, ass,
			ass->GetUnit()->Build(buildDef->GetUnitDef(), buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame);
		)
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
	buildPower -= unit->GetBuildSpeed();
	if ((buildDef != nullptr) && !manager->GetCircuit()->GetEconomyManager()->IsIgnorePull(this)) {
		manager->DelMetalPull(unit);
	}
}

void IBuilderTask::ShowAssignee(CCircuitUnit* unit)
{
	buildPower += unit->GetBuildSpeed();
	if ((buildDef != nullptr) && !manager->GetCircuit()->GetEconomyManager()->IsIgnorePull(this)) {
		manager->AddMetalPull(unit);
	}
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

void IBuilderTask::ExecuteChain(SBuildChain* chain)
{
	assert(chain != nullptr);
	CCircuitAI* circuit = manager->GetCircuit();

	if (chain->energy > 0.f) {
		float energyMake;
		CCircuitDef* energyDef = circuit->GetEconomyManager()->GetLowEnergy(buildPos, energyMake);
		if (energyDef != nullptr) {
			bool isValid = (circuit->GetEconomyManager()->GetAvgEnergyIncome() < chain->energy);
			if (isValid && chain->isMexEngy) {
				int index = circuit->GetMetalManager()->FindNearestSpot(buildPos);
				isValid = (index >= 0) && (circuit->GetMetalManager()->GetSpots()[index].income > energyMake * 0.8f);
			}
			if (isValid) {
				circuit->GetBuilderManager()->EnqueueTask(IBuilderTask::Priority::NORMAL, energyDef, buildPos,
														  IBuilderTask::BuildType::ENERGY, SQUARE_SIZE * 8.0f, true);
			}
		}
	}

	if (chain->isPylon) {
		bool foundPylon = false;
		CEconomyManager* economyManager = circuit->GetEconomyManager();
		CCircuitDef* pylonDef = economyManager->GetPylonDef();
		if (pylonDef->IsAvailable()) {
			float ourRange = economyManager->GetEnergyGrid()->GetPylonRange(buildDef->GetId());
			float pylonRange = economyManager->GetPylonRange();
			float radius = pylonRange + ourRange;
			int frame = circuit->GetLastFrame();
			circuit->UpdateFriendlyUnits();
			auto units = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(buildPos, radius));
			for (Unit* u : units) {
				CCircuitUnit* p = circuit->GetFriendlyUnit(u);
				if (p == nullptr) {
					continue;
				}
				// NOTE: Is SqDistance2D necessary? Or must subtract model radius of pylon from "radius" variable
				//        @see rts/Sim/Misc/QaudField.cpp
				//        ...CQuadField::GetUnitsExact(const float3& pos, float radius, bool spherical)
				//        const float totRad = radius + u->radius; -- suspicious
				if ((*p->GetCircuitDef() == *pylonDef) && (buildPos.SqDistance2D(p->GetPos(frame)) < SQUARE(radius))) {
					foundPylon = true;
					break;
				}
			}
			utils::free_clear(units);
			if (!foundPylon) {
				AIFloat3 pos = buildPos;
				CMetalManager* metalManager = circuit->GetMetalManager();
				int index = metalManager->FindNearestCluster(pos);
				if (index >= 0) {
					const AIFloat3& clPos = metalManager->GetClusters()[index].geoCentr;
					AIFloat3 dir = clPos - pos;
					float dist = ourRange /*+ pylonRange*/ + pylonRange * 1.8f;
					if (dir.SqLength2D() < dist * dist) {
						pos = (pos /*+ dir.Normalize2D() * (ourRange - pylonRange)*/ + clPos) * 0.5f;
					} else {
						pos += dir.Normalize2D() * (ourRange + pylonRange) * 0.9f;
					}
				}
				circuit->GetBuilderManager()->EnqueuePylon(IBuilderTask::Priority::HIGH, pylonDef, pos, nullptr, 1.0f);
			}
		}
	}

	if (chain->isPorc) {
		CEconomyManager* economyManager = circuit->GetEconomyManager();
		const float metalIncome = std::min(economyManager->GetAvgMetalIncome(), economyManager->GetAvgEnergyIncome());
		if (metalIncome > 10) {
			circuit->GetMilitaryManager()->MakeDefence(buildPos);
		} else {
			CMetalManager* metalManager = circuit->GetMetalManager();
			int index = metalManager->FindNearestCluster(buildPos);
			if ((index >= 0) && (metalManager->IsClusterQueued(index) || metalManager->IsClusterFinished(index))) {
				circuit->GetMilitaryManager()->MakeDefence(index, buildPos);
			}
		}
	}

	if (chain->isTerra) {
		if (circuit->GetEconomyManager()->GetAvgMetalIncome() > 10) {
			circuit->GetBuilderManager()->EnqueueTerraform(IBuilderTask::Priority::HIGH, target);
		}
	}

	if (!chain->hub.empty()) {
		// TODO: Implement BuildWait action - semaphore for group of tasks / task's queue
		// FIXME: Using builder's def because MaxSlope is not provided by engine's interface for buildings!
		//        and CTerrainManager::CanBuildAt returns false in many cases
		CCircuitDef* bdef = units.empty() ? circuit->GetSetupManager()->GetCommChoice() : (*this->units.begin())->GetCircuitDef();
		CBuilderManager* builderManager = circuit->GetBuilderManager();
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		CMilitaryManager* militaryManager = circuit->GetMilitaryManager();

		for (auto& queue : chain->hub) {
			IBuilderTask* parent = nullptr;

			for (const SBuildInfo& bi : queue) {
				if (!bi.cdef->IsAvailable()) {
					continue;
				}
				bool isValid = true;
				switch (bi.condition) {
					case SBuildInfo::Condition::AIR: {
						isValid = bi.cdef->GetCost() < militaryManager->GetEnemyCost(CCircuitDef::RoleType::AIR);
					} break;
					case SBuildInfo::Condition::NO_AIR: {
						isValid = bi.cdef->GetCost() > militaryManager->GetEnemyCost(CCircuitDef::RoleType::AIR);
					} break;
					case SBuildInfo::Condition::MAYBE: {
						isValid = rand() < RAND_MAX / 2;
					} break;
					case SBuildInfo::Condition::ALWAYS:
					default: break;
				}
				if (!isValid) {
					continue;
				}

				AIFloat3 offset = bi.offset;
				if (bi.direction != SBuildInfo::Direction::NONE) {
					switch (facing) {
						default:
						case UNIT_FACING_SOUTH:
							break;
						case UNIT_FACING_EAST:
							offset = AIFloat3(offset.z, 0.f, -offset.x);
							break;
						case UNIT_FACING_NORTH:
							offset = AIFloat3(-offset.x, 0.f, -offset.z);
							break;
						case UNIT_FACING_WEST:
							offset = AIFloat3(-offset.z, 0.f, offset.x);
							break;
					}
				}
				AIFloat3 pos = buildPos + offset;
				CTerrainManager::CorrectPosition(pos);
				pos = terrainManager->GetBuildPosition(bdef, pos);

				if (parent == nullptr) {
					parent = builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, bi.cdef, pos, bi.buildType, 0.f, true, 0);
				} else {
					parent->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, bi.cdef, pos, bi.buildType, 0.f, false, 0));
					parent = parent->GetNextTask();
				}
			}
		}
	}
}

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, position);		\
	utils::binary_##func(stream, shake);		\
	utils::binary_##func(stream, bdefId);		\
	utils::binary_##func(stream, buildType);

void IBuilderTask::Load(std::istream& is)
{
	CCircuitDef::Id bdefId;

	IUnitTask::Load(is);
	SERIALIZE(is, read)

	buildDef = manager->GetCircuit()->GetCircuitDef(bdefId);
}

void IBuilderTask::Save(std::ostream& os) const
{
	CCircuitDef::Id bdefId = (buildDef != nullptr) ? buildDef->GetId() : -1;

	IUnitTask::Save(os);
	SERIALIZE(os, write)
}

} // namespace circuit
