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
#include "unit/action/CaptureAction.h"
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
	{"geo",     IBuilderTask::BuildType::GEO},
	{"defence", IBuilderTask::BuildType::DEFENCE},
	{"bunker",  IBuilderTask::BuildType::BUNKER},
	{"big_gun", IBuilderTask::BuildType::BIG_GUN},
	{"radar",   IBuilderTask::BuildType::RADAR},
	{"sonar",   IBuilderTask::BuildType::SONAR},
	{"convert", IBuilderTask::BuildType::CONVERT},
	{"mex",     IBuilderTask::BuildType::MEX},
	{"mexup",   IBuilderTask::BuildType::MEXUP},
};

IBuilderTask::IBuilderTask(ITaskManager* mgr, Priority priority,
						   CCircuitDef* buildDef, const AIFloat3& position,
						   Type type, BuildType buildType, float cost, float shake, int timeout)
		: IUnitTask(mgr, priority, type, timeout)
		, buildType(buildType)
		, position(position)
		, shake(shake)
		, buildDef(buildDef)
		, buildPower(.0f)
		, cost(cost)
		, target(nullptr)
		, buildPos(-RgtVector)
		, facing(UNIT_NO_FACING)
		, nextTask(nullptr)
		, initiator(nullptr)
		, buildFails(0)
		, unitIt(units.end())
{
	CEconomyManager* economyMgr = manager->GetCircuit()->GetEconomyManager();
	savedIncomeM = economyMgr->GetAvgMetalIncome();
	savedIncomeE = economyMgr->GetAvgEnergyIncome();
}

IBuilderTask::IBuilderTask(ITaskManager* mgr, Type type, BuildType buildType)
		: IUnitTask(mgr, type)
		, buildType(buildType)
		, position(-RgtVector)
		, shake(.0f)
		, buildDef(nullptr)
		, buildPower(.0f)
		, cost(.0f)
		, target(nullptr)
		, buildPos(-RgtVector)
		, facing(UNIT_NO_FACING)
		, nextTask(nullptr)
		, initiator(nullptr)
		, savedIncomeM(.0f)
		, savedIncomeE(.0f)
		, buildFails(0)
		, unitIt(units.end())
{
}

IBuilderTask::~IBuilderTask()
{
	delete nextTask;
}

bool IBuilderTask::CanAssignTo(CCircuitUnit* unit) const
{
	// is extra buildpower required?
	const float metalIncome = manager->GetCircuit()->GetEconomyManager()->GetAvgMetalIncome();
	if (cost < buildPower * buildDef->GetGoalBuildTime(metalIncome)) {
		return false;
	}
	const CCircuitDef* cdef = unit->GetCircuitDef();
	// can unit build at all
	if (!cdef->CanBuild(buildDef) && ((target == nullptr) || !cdef->IsAbleToAssist() || cdef->IsAttrSolo())) {
		return false;
	}
	// solo/initiator check
	return !cdef->IsAttrSolo() || (initiator == unit) || ((initiator == nullptr) && (target == nullptr));
}

void IBuilderTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	ShowAssignee(unit);
	if (!utils::is_valid(position)) {
		position = unit->GetPos(circuit->GetLastFrame());
	}
	if (unit->GetCircuitDef()->IsAttrSolo()) {
		initiator = unit;
	}

	if (unit->HasDGun()) {
		const float range = std::max(unit->GetDGunRange(), unit->GetCircuitDef()->GetLosRadius());
		unit->PushDGunAct(new CDGunAction(unit, range));
	}
	if (unit->GetCircuitDef()->IsAbleToCapture()) {
		unit->PushBack(new CCaptureAction(unit, 500.f));
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
	if (initiator == unit) {
		initiator = nullptr;
	}

	IUnitTask::RemoveAssignee(unit);
	traveled.erase(unit);
	executors.erase(unit);

	HideAssignee(unit);
}

void IBuilderTask::Start(CCircuitUnit* unit)
{
	Update(unit);
}

void IBuilderTask::Update()
{
	for (CCircuitUnit* unit : traveled) {
		Execute(unit);
	}
	traveled.clear();

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
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	if (buildDef != nullptr) {
		SBuildChain* chain = builderMgr->GetBuildChain(buildType, buildDef);
		if (chain != nullptr) {
			ExecuteChain(chain);
		}

		const int buildDelay = circuit->GetEconomyManager()->GetBuildDelay();
		if (buildDelay > 0) {
			IUnitTask* task = builderMgr->EnqueueWait(buildDelay);
			decltype(units) tmpUnits = units;
			for (CCircuitUnit* unit : tmpUnits) {
				manager->AssignTask(unit, task);
			}
		}
	}

	// Advance queue
	if (nextTask != nullptr) {
		builderMgr->ActivateTask(nextTask);
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
	executors.insert(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	TRY_UNIT(circuit, unit,
		unit->CmdPriority(ClampPriority());
	)

	const int frame = circuit->GetLastFrame();
	if (target != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->CmdRepair(target, UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
		)
		return;
	}
	if (utils::is_valid(buildPos)) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildDef->GetDef(), buildPos, facing)) {
			TRY_UNIT(circuit, unit,
				unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
			)
			return;
		}
	}

	// FIXME: Move to Reevaluate
	circuit->GetThreatMap()->SetThreatType(unit);
	// FIXME: Replace const 999.0f with build time?
	if (circuit->IsAllyAware() && (cost > 999.0f)) {
		circuit->UpdateFriendlyUnits();
		auto& friendlies = circuit->GetCallback()->GetFriendlyUnitsIn(position, cost);
		CAllyUnit* alu = FindSameAlly(unit, friendlies);
		utils::free(friendlies);
		if (alu != nullptr) {
			TRY_UNIT(circuit, unit,
				unit->CmdRepair(alu, UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
			)
			return;
		}
	}

	// Alter/randomize position
	AIFloat3 pos = (shake > .0f) ? utils::get_near_pos(position, shake) : position;
	CTerrainManager::CorrectPosition(pos);

	const float searchRadius = 200 * SQUARE_SIZE;
	FindBuildSite(unit, pos, searchRadius);

	if (utils::is_valid(buildPos)) {
		TRY_UNIT(circuit, unit,
			unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
		)
	} else {
		if (circuit->GetSetupManager()->GetBasePos().SqDistance2D(position) < SQUARE(searchRadius)) {  // base must be full
			// TODO: Select new proper BasePos, like near metal cluster.
			CTerrainManager* terrainMgr = circuit->GetTerrainManager();
			int terWidth = terrainMgr->GetTerrainWidth();
			int terHeight = terrainMgr->GetTerrainHeight();
			float x = terWidth / 4 + rand() % (int)(terWidth / 2);
			float z = terHeight / 4 + rand() % (int)(terHeight / 2);
			AIFloat3 pos(x, circuit->GetMap()->GetElevationAt(x, z), z);
			circuit->GetSetupManager()->SetBasePos(pos);
		}

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
	traveled.insert(unit);
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
	CTerrainManager* terrainMgr = manager->GetCircuit()->GetTerrainManager();
	if (utils::is_valid(buildPos)) {
		terrainMgr->DelBlocker(buildDef, buildPos, facing);
	}
	buildPos = pos;
	if (utils::is_valid(buildPos)) {
		terrainMgr->AddBlocker(buildDef, buildPos, facing);
	}
}

void IBuilderTask::SetTarget(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	if (utils::is_valid(buildPos)) {
		terrainMgr->DelBlocker(buildDef, buildPos, facing);
	}
	target = unit;
	if (unit != nullptr) {
		facing = unit->GetUnit()->GetBuildingFacing();
		buildDef = unit->GetCircuitDef();
		buildPos = unit->GetPos(circuit->GetLastFrame()) + buildDef->GetMidPosOffset(facing);
	} else {
		buildPos = -RgtVector;
	}
	if (utils::is_valid(buildPos)) {
		terrainMgr->AddBlocker(buildDef, buildPos, facing);
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
			ass->CmdRepair(unit, UNIT_CMD_OPTION, frame);
		)
	}
}

bool IBuilderTask::IsEqualBuildPos(CCircuitUnit* unit) const
{
	AIFloat3 pos = unit->GetPos(manager->GetCircuit()->GetLastFrame());
	pos += unit->GetCircuitDef()->GetMidPosOffset(unit->GetUnit()->GetBuildingFacing());
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
	CEconomyManager* ecoMgr = circuit->GetEconomyManager();
	if ((cost > 1000.0f)
		&& (target == nullptr)
		&& (((ecoMgr->GetAvgMetalIncome() < savedIncomeM * 0.6f) && (ecoMgr->GetAvgMetalIncome() * 2.0f < ecoMgr->GetMetalPull()))
			|| ((ecoMgr->GetAvgEnergyIncome() < savedIncomeE * 0.6f) && (ecoMgr->GetAvgEnergyIncome() * 2.0f < ecoMgr->GetEnergyPull())))
		)
	{
		manager->AbortTask(this);
		return false;
	}

	/*
	 * Reassign task if required
	 */
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	const float sqDist = pos.SqDistance2D(GetPosition());
	if (sqDist <= SQUARE(unit->GetCircuitDef()->GetBuildDistance() + circuit->GetPathfinder()->GetSquareSize())
		&& (circuit->GetInflMap()->GetInfluenceAt(pos) > -INFL_EPS))
	{
//		if (unit->GetCircuitDef()->IsRoleComm()) {  // FIXME: or any other builder-attacker
//			if (circuit->GetInflMap()->GetEnemyInflAt(circuit->GetSetupManager()->GetBasePos()) < INFL_EPS) {
//				return true;
//			}
//		} else {
//			return true;
//		}
		if ((buildType != BuildType::GUARD)
			&& ((executors.size() < 2) || !unit->IsAttrBase()))
		{
			TRY_UNIT(circuit, unit,
				const bool prio = !ecoMgr->IsEnergyStalling() || (buildType == BuildType::ENERGY) || (buildType == BuildType::GEO);
				unit->CmdBARPriority(prio ? 1.f : 0.f);
				if (unit->GetTravelAct()->IsFinished()) {
					unit->CmdWait(ecoMgr->IsEnergyEmpty() && (buildType != BuildType::ENERGY) && (buildType != BuildType::GEO)
							&& (buildType != BuildType::STORE) && (buildType != BuildType::RECLAIM));
				}
			)
			return true;
		}
	} else {
		// Remove wait if unit was pushed away from build position
		TRY_UNIT(circuit, unit,
			unit->CmdWait(false);
		)
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
	// TODO: Check IsForceUpdate, shield charge and retreat

	CCircuitDef* cdef = unit->GetCircuitDef();
	const float range = cdef->GetBuildDistance();
	const AIFloat3& endPos = GetPosition();
	if ((target == nullptr)
		&& !circuit->GetTerrainManager()->CanReachAtSafe(unit, endPos, range, cdef->GetPower()))
	{
		manager->AbortTask(this);
		return;
	}

	const int frame = circuit->GetLastFrame();
	const AIFloat3& startPos = unit->GetPos(frame);

	if ((startPos.SqDistance2D(endPos) < SQUARE(range))
		|| ((circuit->GetSetupManager()->GetBasePos().SqDistance2D(startPos) < SQUARE(circuit->GetMilitaryManager()->GetBaseDefRange()))
			&& (circuit->GetSetupManager()->GetBasePos().SqDistance2D(endPos) < SQUARE(circuit->GetMilitaryManager()->GetBaseDefRange()))))
	{
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

	pathfinder->RunQuery(query, [this](const IPathQuery* query) {
		this->ApplyPath(static_cast<const CQueryPathSingle*>(query));
	});
}

void IBuilderTask::ApplyPath(const CQueryPathSingle* query)
{
	const std::shared_ptr<PathInfo>& pPath = query->GetPathInfo();
	CCircuitUnit* unit = query->GetUnit();

	if (pPath->path.size() > 2) {
		unit->GetTravelAct()->SetPath(pPath);
	} else {
		unit->GetTravelAct()->StateFinish();
	}
}

void IBuilderTask::HideAssignee(CCircuitUnit* unit)
{
	if (buildDef == nullptr) {
		buildPower -= unit->GetBuildSpeed();
	} else {
		const float buildTime = buildDef->GetBuildTime() / unit->GetWorkerTime();
		const float metalRequire = buildDef->GetCostM() / buildTime;
		buildPower -= metalRequire;
		if (!manager->GetCircuit()->GetEconomyManager()->IsIgnorePull(this)) {
			manager->DelMetalPull(metalRequire);
		}
	}
}

void IBuilderTask::ShowAssignee(CCircuitUnit* unit)
{
	if (buildDef == nullptr) {
		buildPower += unit->GetBuildSpeed();
	} else {
		const float buildTime = buildDef->GetBuildTime() / unit->GetWorkerTime();
		const float metalRequire = buildDef->GetCostM() / buildTime;
		buildPower += metalRequire;
		if (!manager->GetCircuit()->GetEconomyManager()->IsIgnorePull(this)) {
			manager->AddMetalPull(metalRequire);
		}
	}
}

CAllyUnit* IBuilderTask::FindSameAlly(CCircuitUnit* builder, const std::vector<Unit*>& friendlies)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	const int frame = circuit->GetLastFrame();

	for (Unit* au : friendlies) {
		CAllyUnit* alu = circuit->GetFriendlyUnit(au);
		if (alu == nullptr) {
			continue;
		}
		if ((*alu->GetCircuitDef() == *buildDef) && au->IsBeingBuilt()) {
			const AIFloat3& pos = alu->GetPos(frame);
			if (terrainMgr->CanReachAtSafe(builder, pos, builder->GetCircuitDef()->GetBuildDistance())) {
				return alu;
			}
		}
	}
	return nullptr;
}

void IBuilderTask::FindBuildSite(CCircuitUnit* builder, const AIFloat3& pos, float searchRadius)
{
	FindFacing(pos);

	CTerrainManager* terrainMgr = manager->GetCircuit()->GetTerrainManager();
	CTerrainManager::TerrainPredicate predicate = [terrainMgr, builder](const AIFloat3& p) {
		return terrainMgr->CanReachAtSafe(builder, p, builder->GetCircuitDef()->GetBuildDistance());
	};
	SetBuildPos(terrainMgr->FindBuildSite(buildDef, pos, searchRadius, facing, predicate));
}

void IBuilderTask::FindFacing(const springai::AIFloat3& pos)
{
	CTerrainManager* terrainMgr = manager->GetCircuit()->GetTerrainManager();

//	facing = UNIT_NO_FACING;
	float terWidth = terrainMgr->GetTerrainWidth();
	float terHeight = terrainMgr->GetTerrainHeight();
	if (std::fabs(terWidth - 2 * pos.x) > std::fabs(terHeight - 2 * pos.z)) {
		facing = (2 * pos.x > terWidth) ? UNIT_FACING_WEST : UNIT_FACING_EAST;
	} else {
		facing = (2 * pos.z > terHeight) ? UNIT_FACING_NORTH : UNIT_FACING_SOUTH;
	}
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
				isValid = (index >= 0) && (circuit->GetMetalManager()->GetSpots()[index].income * buildDef->GetExtractsM() > energyMake * 0.8f);
			}
			if (isValid) {
				circuit->GetBuilderManager()->EnqueueTask(IBuilderTask::Priority::NORMAL, energyDef, buildPos,
														  IBuilderTask::BuildType::ENERGY, SQUARE_SIZE * 8.0f, true);
			}
		}
	}

	if (chain->isPylon) {
		bool foundPylon = false;
		CEconomyManager* economyMgr = circuit->GetEconomyManager();
		CCircuitDef* pylonDef = economyMgr->GetPylonDef();
		if (pylonDef->IsAvailable(circuit->GetLastFrame())) {
			float ourRange = economyMgr->GetEnergyGrid()->GetPylonRange(buildDef->GetId());
			float pylonRange = economyMgr->GetPylonRange();
			float radius = pylonRange + ourRange;
			const int frame = circuit->GetLastFrame();
			circuit->UpdateFriendlyUnits();
			auto& units = circuit->GetCallback()->GetFriendlyUnitsIn(buildPos, radius);
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
			utils::free(units);
			if (!foundPylon) {
				AIFloat3 pos = buildPos;
				CMetalManager* metalMgr = circuit->GetMetalManager();
				int index = metalMgr->FindNearestCluster(pos);
				if (index >= 0) {
					const AIFloat3& clPos = metalMgr->GetClusters()[index].position;
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
//		CEconomyManager* economyMgr = circuit->GetEconomyManager();
//		const float metalIncome = std::min(economyMgr->GetAvgMetalIncome(), economyMgr->GetAvgEnergyIncome());
//		if (metalIncome > 10) {
			circuit->GetMilitaryManager()->MakeDefence(buildPos);
//		} else {
//			CMetalManager* metalMgr = circuit->GetMetalManager();
//			int index = metalMgr->FindNearestCluster(buildPos);
//			if ((index >= 0) && (/*metalMgr->IsClusterQueued(index) || */metalMgr->IsClusterFinished(index))) {
//				circuit->GetMilitaryManager()->MakeDefence(index, buildPos);
//			}
//		}
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
		CBuilderManager* builderMgr = circuit->GetBuilderManager();
		CTerrainManager* terrainMgr = circuit->GetTerrainManager();
		CEnemyManager* enemyMgr = circuit->GetEnemyManager();

		for (auto& queue : chain->hub) {
			IBuilderTask* parent = nullptr;

			for (const SBuildInfo& bi : queue) {
				if (!bi.cdef->IsAvailable(circuit->GetLastFrame())
					|| !terrainMgr->GetImmobileTypeById(bi.cdef->GetImmobileId())->typeUsable)
				{
					continue;
				}
				bool isValid = true;
				switch (bi.condition) {
					case SBuildInfo::Condition::AIR: {
						isValid = bi.cdef->GetCostM() < enemyMgr->GetEnemyCost(ROLE_TYPE(AIR));
						if (bi.value < 0.f) {  // -1.f == false
							isValid = !isValid;
						}
					} break;
					case SBuildInfo::Condition::ENERGY: {
						CEconomyManager* ecoMgr = circuit->GetEconomyManager();
						// isValid = !ecoMgr->IsEnergyStalling() && (ecoMgr->GetAvgEnergyIncome() > ecoMgr->GetEnergyPull() + bi.cdef->GetUpkeepE());
						isValid = !ecoMgr->IsEnergyStalling() && ecoMgr->IsEnergyFull();
						if (bi.value < 0.f) {  // -1.f == false
							isValid = !isValid;
						}
					} break;
					case SBuildInfo::Condition::WIND: {
						CEconomyManager* ecoMgr = circuit->GetEconomyManager();
						isValid = ecoMgr->IsEnergyStalling() || (ecoMgr->GetAvgEnergyIncome() < ecoMgr->GetEnergyPull() + bi.cdef->GetUpkeepE());
						float avgWind = (circuit->GetMap()->GetMaxWind() + circuit->GetMap()->GetMinWind()) * 0.5f;
						isValid = isValid && (avgWind >= bi.value);
					} break;
					case SBuildInfo::Condition::SENSOR: {
						std::function<bool (CCircuitDef*)> isSensor;
						if (bi.cdef->IsRadar()) {
							isSensor = [](CCircuitDef* cdef) { return cdef->IsRadar(); };
						} else if (bi.cdef->IsSonar()) {
							isSensor = [](CCircuitDef* cdef) { return cdef->IsSonar(); };
						} else {
							isValid = false;
							break;
						}
						COOAICallback* clb = circuit->GetCallback();
						const auto& friendlies = clb->GetFriendlyUnitIdsIn(buildPos, bi.value);
						for (int auId : friendlies) {
							if (auId == -1) {
								continue;
							}
							CCircuitDef::Id defId = clb->Unit_GetDefId(auId);
							if (isSensor(circuit->GetCircuitDef(defId))) {
								isValid = false;
								break;
							}
						}
					} break;
					case SBuildInfo::Condition::CHANCE: {
						isValid = rand() < bi.value * RAND_MAX;
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
				pos = terrainMgr->GetBuildPosition(bdef, pos);

				if (parent == nullptr) {
					parent = builderMgr->EnqueueTask(bi.priority, bi.cdef, pos, bi.buildType, 0.f, true, 0);
				} else {
					parent->SetNextTask(builderMgr->EnqueueTask(bi.priority, bi.cdef, pos, bi.buildType, 0.f, false, 0));
					parent = parent->GetNextTask();
				}
			}
		}
	}
}

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, positionF3);	\
	utils::binary_##func(stream, shake);		\
	utils::binary_##func(stream, bdefId);		\
	utils::binary_##func(stream, cost);			\
	utils::binary_##func(stream, targetId);		\
	utils::binary_##func(stream, buildPosF3);	\
	utils::binary_##func(stream, facing);		\
	utils::binary_##func(stream, savedIncomeM);	\
	utils::binary_##func(stream, savedIncomeE);	\
	utils::binary_##func(stream, buildFails);

bool IBuilderTask::Load(std::istream& is)
{
	CCircuitDef::Id bdefId;
	CCircuitUnit::Id targetId;
	float positionF3[3];
	float buildPosF3[3];

	IUnitTask::Load(is);
	SERIALIZE(is, read)

	CCircuitAI* circuit = manager->GetCircuit();
	buildDef = circuit->GetCircuitDefSafe(bdefId);
	target = circuit->GetTeamUnit(targetId);
	position = AIFloat3(positionF3);
	buildPos = AIFloat3(buildPosF3);

	if ((target != nullptr) && (buildType != BuildType::REPAIR) && (buildType != BuildType::RECLAIM)) {
		circuit->GetBuilderManager()->MarkUnfinishedUnit(target, this);
	}
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | position=%f,%f,%f | shake=%f | bdefId=%i | cost=%f | targetId=%i | buildPos=%f,%f,%f | facing=%i | savedIncomeM=%f | savedIncomeE=%f | buildFails=%i | buildDef=%lx | target=%lx",
			__PRETTY_FUNCTION__, position.x, position.y, position.z, shake, bdefId, cost, targetId, buildPos.x, buildPos.y, buildPos.z, facing, savedIncomeM, savedIncomeE, buildFails, buildDef, target);
#endif
	return true;
}

void IBuilderTask::Save(std::ostream& os) const
{
	CCircuitDef::Id bdefId = (buildDef != nullptr) ? buildDef->GetId() : -1;
	CCircuitUnit::Id targetId = (target != nullptr) ? target->GetId() : -1;
	float positionF3[3];
	float buildPosF3[3];
	position.LoadInto(positionF3);
	buildPos.LoadInto(buildPosF3);

	IUnitTask::Save(os);
	SERIALIZE(os, write)
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | positionF3=%f,%f,%f | shake=%f | bdefId=%i | cost=%f | targetId=%i | buildPosF3=%f,%f,%f | facing=%i | savedIncomeM=%f | savedIncomeE=%f | buildFails=%i",
			__PRETTY_FUNCTION__, positionF3[0], positionF3[1], positionF3[2], shake, bdefId, cost, targetId, buildPosF3[0], buildPosF3[1], buildPosF3[2], facing, savedIncomeM, savedIncomeE, buildFails);
#endif
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
