/*
 * BuilderTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "task/builder/BuilderTask.h"
#include "task/builder/BuildChain.h"
#include "task/RetreatTask.h"
#include "map/ThreatMap.h"
#include "map/InfluenceMap.h"
#include "module/EconomyManager.h"
#include "module/BuilderManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "resource/EnergyGrid.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/path/PathFinder.h"
#include "terrain/path/QueryPathSingle.h"
#include "unit/action/DGunAction.h"
#include "unit/action/FightAction.h"
#include "unit/action/MoveAction.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "AISCommands.h"

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

	CCircuitAI* circuit = manager->GetCircuit();
	ShowAssignee(unit);
	if (!utils::is_valid(position)) {
		position = unit->GetPos(circuit->GetLastFrame());
	}

	if (unit->HasDGun()) {
		unit->PushDGunAct(new CDGunAction(unit, unit->GetDGunRange()));
	}

	// NOTE: only for unit->GetCircuitDef()->IsMobile()
	int squareSize = circuit->GetPathfinder()->GetSquareSize();
	ITravelAction* travelAction;
	if (unit->GetCircuitDef()->IsAttrSiege()) {
		travelAction = new CFightAction(unit, squareSize);
	} else {
		travelAction = new CMoveAction(unit, squareSize);
	}
	unit->PushTravelAct(travelAction);
	travelAction->StateWait();
}

void IBuilderTask::RemoveAssignee(CCircuitUnit* unit)
{
	if ((units.find(unit) == unitIt) && (unitIt != units.end())) {
		++unitIt;
	}
	IUnitTask::RemoveAssignee(unit);

	HideAssignee(unit);
}

void IBuilderTask::Start(CCircuitUnit* unit)
{
	Update(unit);
}

void IBuilderTask::Update()
{
	CCircuitUnit* unit = GetNextAssignee();
	if (unit == nullptr) {
		return;
	}

	Update(unit);
}

void IBuilderTask::Stop(bool done)
{
	IUnitTask::Stop(done);

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
		SetBuildPos(-RgtVector);
	}

	// Destructor will take care of the nextTask queue
}

void IBuilderTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	TRY_UNIT(circuit, unit,
		unit->CmdPriority(ClampPriority());
	)

	const int frame = circuit->GetLastFrame();
	if (target != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Repair(target->GetUnit(), UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
		)
		return;
	}
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	UnitDef* buildUDef = buildDef->GetDef();
	if (utils::is_valid(buildPos)) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Build(buildUDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
			)
			return;
		}
	}

	// FIXME: Move to Reevaluate
	circuit->GetThreatMap()->SetThreatType(unit);
	// FIXME: Replace const 999.0f with build time?
	if (circuit->IsAllyAware() && (cost > 999.0f)) {
		circuit->UpdateFriendlyUnits();
		auto friendlies = circuit->GetCallback()->GetFriendlyUnitsIn(position, cost);
		CAllyUnit* alu = FindSameAlly(unit, friendlies);
		utils::free_clear(friendlies);
		if (alu != nullptr) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Repair(alu->GetUnit(), UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
			)
			return;
		}
	}

	// Alter/randomize position
	AIFloat3 pos = (shake > .0f) ? utils::get_near_pos(position, shake) : position;

	const float searchRadius = 200.0f * SQUARE_SIZE;
	FindBuildSite(unit, pos, searchRadius);

	if (utils::is_valid(buildPos)) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Build(buildUDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
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

void IBuilderTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float healthPerc = unit->GetHealthPercent();
	if ((healthPerc > cdef->GetRetreat()) && !unit->IsDisarmed(frame)) {
		return;
	}

	CRetreatTask* task = circuit->GetBuilderManager()->EnqueueRetreat();
	manager->AssignTask(unit, task);

	if (target == nullptr) {
		manager->AbortTask(this);  // Doesn't call RemoveAssignee
	}
}

void IBuilderTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	RemoveAssignee(unit);
	// NOTE: AbortTask usually does not call RemoveAssignee for each unit
	if (((target == nullptr) || units.empty()) && !unit->IsMorphing()) {
		manager->AbortTask(this);
	}
}

void IBuilderTask::OnTravelEnd(CCircuitUnit* unit)
{
	Execute(unit);
}

void IBuilderTask::Activate()
{
	lastTouched = manager->GetCircuit()->GetLastFrame();
}

void IBuilderTask::Deactivate()
{
	lastTouched = -1;
}

void IBuilderTask::SetBuildPos(const AIFloat3& pos)
{
	CTerrainManager* terrainManager = manager->GetCircuit()->GetTerrainManager();
	if (utils::is_valid(buildPos)) {
		terrainManager->DelBlocker(buildDef, buildPos, facing);
	}
	buildPos = pos;
	if (utils::is_valid(buildPos)) {
		terrainManager->AddBlocker(buildDef, buildPos, facing);
	}
}

void IBuilderTask::SetTarget(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	if (utils::is_valid(buildPos)) {
		terrainManager->DelBlocker(buildDef, buildPos, facing);
	}
	target = unit;
	if (unit != nullptr) {
		buildDef = unit->GetCircuitDef();
		buildPos = unit->GetPos(circuit->GetLastFrame());
		facing = unit->GetUnit()->GetBuildingFacing();
	} else {
		buildPos = -RgtVector;
	}
	if (utils::is_valid(buildPos)) {
		terrainManager->AddBlocker(buildDef, buildPos, facing);
	}
}

void IBuilderTask::UpdateTarget(CCircuitUnit* unit)
{
	// NOTE: unit->GetPos() may differ from buildPos
	SetTarget(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame() + FRAMES_PER_SEC * 60;
	for (CCircuitUnit* ass : units) {
		TRY_UNIT(circuit, ass,
			ass->GetUnit()->Repair(unit->GetUnit(), UNIT_CMD_OPTION, frame);
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

CCircuitUnit* IBuilderTask::GetNextAssignee()
{
	if (units.empty()) {
		return nullptr;
	}
	if (unitIt == units.end()) {
		unitIt = units.begin();
	}
	CCircuitUnit* unit = *unitIt;
	++unitIt;
	return unit;
}

void IBuilderTask::Update(CCircuitUnit* unit)
{
	if (Reevaluate(unit) && !unit->GetTravelAct()->IsFinished()) {
		UpdatePath(unit);  // Execute(unit) within OnTravelEnd
	}
}

bool IBuilderTask::Reevaluate(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	// TODO: Check for open build site, push mobile units away or temporary block position.

	// FIXME: Replace const 1000.0f with build time?
	CEconomyManager* em = circuit->GetEconomyManager();
	if ((cost > 1000.0f) &&
		(target == nullptr) &&
		(em->GetAvgMetalIncome() < savedIncome * 0.6f) &&
		(em->GetAvgMetalIncome() * 2.0f < em->GetMetalPull()))
	{
		manager->AbortTask(this);
		return false;
	}

	// Reassign task if required
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	const float sqDist = pos.SqDistance2D(GetPosition());
	if (sqDist <= SQUARE(unit->GetCircuitDef()->GetBuildDistance() + circuit->GetPathfinder()->GetSquareSize())
		&& (circuit->GetInflMap()->GetInfluenceAt(pos) > -INFL_EPS))
	{
		if (unit->GetCircuitDef()->IsRoleComm()) {  // FIXME: or any other builder-attacker
			if (circuit->GetInflMap()->GetEnemyInflAt(circuit->GetSetupManager()->GetBasePos()) < INFL_EPS) {
				return true;
			}
		} else {
			return true;
		}
	}
	HideAssignee(unit);
	IUnitTask* task = manager->MakeTask(unit);
	ShowAssignee(unit);
	if ((task != nullptr)
		&& ((task->GetType() != IUnitTask::Type::BUILDER)
			|| (static_cast<IBuilderTask*>(task)->GetBuildType() != buildType)))
	{
		manager->AssignTask(unit, task);
		return false;
	}
	return true;
}

void IBuilderTask::UpdatePath(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	// TODO: Check IsForceExecute, shield charge and retreat

	const AIFloat3& endPos = GetPosition();
	if (!circuit->GetTerrainManager()->CanBuildAtSafe(unit, endPos)) {
		manager->AbortTask(this);
		return;
	}

	const int frame = circuit->GetLastFrame();
	const AIFloat3& startPos = unit->GetPos(frame);
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float range = cdef->GetBuildDistance();

	if (startPos.SqDistance2D(endPos) < SQUARE(range)) {
		unit->GetTravelAct()->StateFinish();
		return;
	}

	if (!IsQueryReady(unit)) {
		return;
	}

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathSingleQuery(
			unit, circuit->GetThreatMap(), frame,
			startPos, endPos, range);
	pathQueries[unit] = query;
	query->HoldTask(this);

	pathfinder->RunQuery(query, [this](const IPathQuery* query) {
		if (this->IsQueryAlive(query)) {
			this->ApplyPath(static_cast<const CQueryPathSingle*>(query));
		}
	});
}

void IBuilderTask::ApplyPath(const CQueryPathSingle* query)
{
	const std::shared_ptr<PathInfo>& pPath = query->GetPathInfo();
	CCircuitUnit* unit = query->GetUnit();

	if (pPath->path.size() > 2) {
		unit->GetTravelAct()->SetPath(pPath);
		unit->GetTravelAct()->StateActivate();
	} else {
		unit->GetTravelAct()->StateFinish();
	}
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

CAllyUnit* IBuilderTask::FindSameAlly(CCircuitUnit* builder, const std::vector<Unit*>& friendlies)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	const int frame = circuit->GetLastFrame();

	for (Unit* au : friendlies) {
		CAllyUnit* alu = circuit->GetFriendlyUnit(au);
		if (alu == nullptr) {
			continue;
		}
		if ((*alu->GetCircuitDef() == *buildDef) && au->IsBeingBuilt()) {
			const AIFloat3& pos = alu->GetPos(frame);
			if (terrainManager->CanBuildAtSafe(builder, pos)) {
				return alu;
			}
		}
	}
	return nullptr;
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
		return terrainManager->CanBuildAtSafe(builder, p);
	};
	SetBuildPos(terrainManager->FindBuildSite(buildDef, pos, searchRadius, facing, predicate));
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
		if (pylonDef->IsAvailable(circuit->GetLastFrame())) {
			float ourRange = economyManager->GetEnergyGrid()->GetPylonRange(buildDef->GetId());
			float pylonRange = economyManager->GetPylonRange();
			float radius = pylonRange + ourRange;
			const int frame = circuit->GetLastFrame();
			circuit->UpdateFriendlyUnits();
			auto units = circuit->GetCallback()->GetFriendlyUnitsIn(buildPos, radius);
			for (Unit* u : units) {
				CAllyUnit* p = circuit->GetFriendlyUnit(u);
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
					const AIFloat3& clPos = metalManager->GetClusters()[index].position;
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
		CEnemyManager* enemyManager = circuit->GetEnemyManager();

		for (auto& queue : chain->hub) {
			IBuilderTask* parent = nullptr;

			for (const SBuildInfo& bi : queue) {
				if (!bi.cdef->IsAvailable(circuit->GetLastFrame())) {
					continue;
				}
				bool isValid = true;
				switch (bi.condition) {
					case SBuildInfo::Condition::AIR: {
						isValid = bi.cdef->GetCost() < enemyManager->GetEnemyCost(ROLE_TYPE(AIR));
					} break;
					case SBuildInfo::Condition::NO_AIR: {
						isValid = bi.cdef->GetCost() > enemyManager->GetEnemyCost(ROLE_TYPE(AIR));
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

#ifdef DEBUG_VIS
void IBuilderTask::Log()
{
	IUnitTask::Log();
	CCircuitAI* circuit = manager->GetCircuit();
	circuit->LOG("buildType: %i", buildType);
	circuit->GetDrawer()->AddPoint(GetPosition(), (buildDef != nullptr) ? buildDef->GetDef()->GetName() : "task");
}
#endif

} // namespace circuit
