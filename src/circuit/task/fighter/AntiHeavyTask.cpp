/*
 * AntiHeavyTask.cpp
 *
 *  Created on: Jun 30, 2016
 *      Author: rlcevg
 */

#include "task/fighter/AntiHeavyTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/path/PathFinder.h"
#include "terrain/path/QueryPathSingle.h"
#include "terrain/path/QueryPathMulti.h"
#include "unit/action/DGunAction.h"
#include "unit/action/FightAction.h"
#include "unit/action/MoveAction.h"
#include "unit/enemy/EnemyUnit.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;
using namespace terrain;

CAntiHeavyTask::CAntiHeavyTask(ITaskManager* mgr, float powerMod)
		: ISquadTask(mgr, FightType::AH, powerMod)
{
	CCircuitAI* circuit = manager->GetCircuit();
	float x = rand() % circuit->GetTerrainManager()->GetTerrainWidth();
	float z = rand() % circuit->GetTerrainManager()->GetTerrainHeight();
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
}

CAntiHeavyTask::~CAntiHeavyTask()
{
}

bool CAntiHeavyTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (!unit->GetCircuitDef()->IsRoleAH() ||
		(unit->GetCircuitDef() != leader->GetCircuitDef()))
	{
		return false;
	}
	const int frame = manager->GetCircuit()->GetLastFrame();
	if (leader->GetPos(frame).SqDistance2D(unit->GetPos(frame)) > SQUARE(1000.f)) {
		return false;
	}
	return true;
}

void CAntiHeavyTask::AssignTo(CCircuitUnit* unit)
{
	ISquadTask::AssignTo(unit);
	CCircuitDef* cdef = unit->GetCircuitDef();
	highestRange = std::max(highestRange, cdef->GetLosRadius());
	highestRange = std::max(highestRange, cdef->GetJumpRange());

	CCircuitAI* circuit = manager->GetCircuit();
	if (cdef->IsAbleToCloak() && !cdef->IsOpenFire()) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->SetFireState(CCircuitDef::FireType::RETURN);
		)
	}

	if (unit->GetDGunAct() != nullptr) {
		unit->GetDGunAct()->StateWait();
	}

	int squareSize = circuit->GetPathfinder()->GetSquareSize();
	ITravelAction* travelAction;
	if (cdef->IsAttrSiege()) {
		travelAction = new CFightAction(unit, squareSize);
	} else {
		travelAction = new CMoveAction(unit, squareSize);
	}
	unit->PushTravelAct(travelAction);
	travelAction->StateWait();
	unit->SetAllowedToJump(cdef->IsAbleToJump() && cdef->IsAttrJump());
}

void CAntiHeavyTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);
	if (leader == nullptr) {
		manager->AbortTask(this);
	} else {
		highestRange = std::max(highestRange, leader->GetCircuitDef()->GetLosRadius());
		highestRange = std::max(highestRange, leader->GetCircuitDef()->GetJumpRange());
	}

	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->IsAbleToCloak()) {
		TRY_UNIT(manager->GetCircuit(), unit,
			unit->GetUnit()->SetFireState(cdef->GetFireState());
		)
	}
}

void CAntiHeavyTask::Start(CCircuitUnit* unit)
{
	if ((State::REGROUP == state) || (State::ENGAGE == state)) {
		return;
	}
	if (!pPath->posPath.empty()) {
		unit->GetTravelAct()->SetPath(pPath);
	}
}

void CAntiHeavyTask::Update()
{
	++updCount;

	/*
	 * Drop task if no enemy heavy exists
	 */
	if (updCount % 32 == 1) {
		if (manager->GetCircuit()->GetEnemyManager()->GetEnemyCost(ROLE_TYPE(HEAVY)) < 1.f) {
			manager->AbortTask(this);
			return;
		}
	}

	/*
	 * Merge tasks if possible
	 */
	ISquadTask* task = GetMergeTask();
	if (task != nullptr) {
		task->Merge(this);
		units.clear();
		manager->AbortTask(this);
		return;
	}

	/*
	 * Regroup if required
	 */
	bool wasRegroup = (State::REGROUP == state);
	bool mustRegroup = IsMustRegroup();
	if (State::REGROUP == state) {
		if (mustRegroup) {
			CCircuitAI* circuit = manager->GetCircuit();
			int frame = circuit->GetLastFrame() + FRAMES_PER_SEC * 60;
			for (CCircuitUnit* unit : units) {
				TRY_UNIT(circuit, unit,
					unit->CmdFightTo(groupPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame);
				)
				unit->GetTravelAct()->StateWait();
			}
		}
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	bool isExecute = (updCount % 2 == 0) && (frame >= lastTouched + FRAMES_PER_SEC);
	if (!isExecute) {
		for (CCircuitUnit* unit : units) {
			isExecute |= unit->IsForceUpdate(frame);
		}
		if (!isExecute) {
			if (wasRegroup && !pPath->posPath.empty()) {
				ActivePath();
			}
			return;
		}
	}
	lastTouched = frame;

	/*
	 * Update target
	 */
	if (!leader->GetCircuitDef()->IsRoleMine()
		&& (leader->GetCircuitDef()->GetReloadTime() >= FRAMES_PER_SEC * 5)
		&& !leader->IsWeaponReady(frame))
	{
		if (IsQueryReady(leader)) {
			FallbackAttackSafe();
		}
		return;
	}

	const bool isTargetsFound = FindTarget();

	state = State::ROAM;
	if (GetTarget() != nullptr) {
		state = State::ENGAGE;
		position = GetTarget()->GetPos();
		float power = 0.f;
		CEnemyInfo* target = GetTarget();
		auto subattack = [&power, target](CCircuitUnit* unit) {
			if (unit->GetDGunAct() != nullptr) {
				unit->GetDGunAct()->StateActivate();
			}
			unit->GetTravelAct()->StateWait();

			if (unit->GetCircuitDef()->IsRoleMine()) {
				const bool isAttack = (target->GetInfluence() > power);
				power += unit->GetCircuitDef()->GetPower();
				return isAttack;
			}
			return true;
		};
		const bool isGroundAttack = target->GetUnit()->IsCloaked();
		for (CCircuitUnit* unit : units) {
			if (subattack(unit)) {
				unit->Attack(target, isGroundAttack, frame + FRAMES_PER_SEC * 60);
			}
		}
		return;
	}

	if (!IsQueryReady(leader)) {
		return;
	}

	if (!isTargetsFound) {  // enemyPositions.empty()
		FallbackTargetEmpty();
		return;
	}

	const AIFloat3& startPos = leader->GetPos(frame);

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathMultiQuery(
			leader, circuit->GetThreatMap(),
			startPos, pathfinder->GetSquareSize(), enemyPositions);
	pathQueries[leader] = query;

	pathfinder->RunQuery(circuit->GetScheduler().get(), query, [this](const IPathQuery* query) {
		this->ApplyTargetPath(static_cast<const CQueryPathMulti*>(query));
	});
}

void CAntiHeavyTask::OnUnitIdle(CCircuitUnit* unit)
{
	ISquadTask::OnUnitIdle(unit);
	if (units.empty()) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	const float maxDist = std::max<float>(lowestRange, circuit->GetPathfinder()->GetSquareSize());
	if (position.SqDistance2D(leader->GetPos(circuit->GetLastFrame())) < SQUARE(maxDist)) {
		CTerrainManager* terrainMgr = circuit->GetTerrainManager();
		float x = rand() % terrainMgr->GetTerrainWidth();
		float z = rand() % terrainMgr->GetTerrainHeight();
		position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
		position = terrainMgr->GetMovePosition(leader->GetArea(), position);
	}

	if (units.find(unit) != units.end()) {
		Start(unit);  // NOTE: Not sure if it has effect
	}
}

void CAntiHeavyTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	ISquadTask::OnUnitDamaged(unit, attacker);

	if (attacker != nullptr) {
		if (unit->GetCircuitDef()->IsAbleToCloak()) {
			TRY_UNIT(manager->GetCircuit(), unit,
				unit->GetUnit()->SetFireState(CCircuitDef::FireType::OPEN);
			)
		}
		if (unit->GetDGunAct() != nullptr) {
			unit->GetDGunAct()->StateActivate();
		}
	}
}

bool CAntiHeavyTask::FindTarget()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CMap* map = circuit->GetMap();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = leader->GetPos(circuit->GetLastFrame());
	SArea* area = leader->GetArea();
	CCircuitDef* cdef = leader->GetCircuitDef();
	const bool notAA = !cdef->HasSurfToAir();
	const int canTargetCat = cdef->GetTargetCategory();
	const float maxPower = attackPower * powerMod;
	const float weaponRange = cdef->GetMaxRange();
	const float range = std::max(highestRange, threatMap->GetSquareSize() * 2.0f);
	const float losSqDist = SQUARE(range);
	float minSqDist = losSqDist;

	CEnemyInfo* bestTarget = nullptr;
	enemyPositions.clear();
	threatMap->SetThreatType(leader);
	const CCircuitAI::EnemyInfos& enemies = circuit->GetEnemyInfos();
	for (auto& kv : enemies) {
		CEnemyInfo* enemy = kv.second;
		if (enemy->IsHidden()) {
			continue;
		}
		const AIFloat3& ePos = enemy->GetPos();
		if ((maxPower <= threatMap->GetThreatAt(ePos)/* - enemy->GetThreat(ROLE_TYPE(AH))*/)
			|| !terrainMgr->CanMoveToPos(area, ePos))
		{
			continue;
		}

		CCircuitDef* edef = enemy->GetCircuitDef();
		if ((edef == nullptr) || !edef->IsEnemyRoleAny(CCircuitDef::RoleMask::HEAVY | CCircuitDef::RoleMask::COMM)
			|| ((edef->GetCategory() & canTargetCat) == 0)
			|| (edef->IsAbleToFly() && notAA)
			|| (ePos.y - map->GetElevationAt(ePos.x, ePos.z) > weaponRange))
		{
			continue;
		}

		const float sqDist = pos.SqDistance2D(ePos);
		if (minSqDist > sqDist) {
			minSqDist = sqDist;
			bestTarget = enemy;
		} else if (losSqDist <= sqDist) {
			enemyPositions.push_back(ePos);
		}
	}

	SetTarget(bestTarget);
	if (bestTarget != nullptr) {
		return true;
	}
	if (enemyPositions.empty()) {
		return false;
	}

	return true;
	// Return: target, startPos=leader->pos, enemyPositions
}

void CAntiHeavyTask::ApplyTargetPath(const CQueryPathMulti* query)
{
	pPath = query->GetPathInfo();

	if (!pPath->posPath.empty()) {
		position = pPath->posPath.back();
		ActivePath();
	} else {
		FallbackTargetEmpty();
	}
}

void CAntiHeavyTask::FallbackTargetEmpty()
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (!leader->GetCircuitDef()->IsRoleMine() &&
		(circuit->GetEnemyManager()->GetEnemyCost(ROLE_TYPE(HEAVY)) < 1.f))
	{
		manager->AbortTask(this);
	} else {
		FallbackAttackSafe();
	}
}

void CAntiHeavyTask::FallbackAttackSafe()
{
	if (leader->GetCircuitDef()->IsRoleMine()) {
		FallbackBasePos();
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	circuit->GetMilitaryManager()->FillAttackSafePos(leader, urgentPositions);
	if (urgentPositions.empty()) {
		FallbackStaticSafe();
		return;
	}

	const AIFloat3& startPos = leader->GetPos(circuit->GetLastFrame());
	const float pathRange = DEFAULT_SLACK * 4;

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathMultiQuery(
			leader, circuit->GetThreatMap(),
			startPos, pathRange, urgentPositions);
	pathQueries[leader] = query;

	pathfinder->RunQuery(circuit->GetScheduler().get(), query, [this](const IPathQuery* query) {
		this->ApplyAttackSafe(static_cast<const CQueryPathMulti*>(query));
	});
}

void CAntiHeavyTask::ApplyAttackSafe(const CQueryPathMulti* query)
{
	pPath = query->GetPathInfo();

	if (!pPath->posPath.empty()) {
		position = pPath->posPath.back();
		ActivePath();
	} else {
		FallbackStaticSafe();
	}
}

void CAntiHeavyTask::FallbackStaticSafe()
{
	CCircuitAI* circuit = manager->GetCircuit();
	circuit->GetMilitaryManager()->FillStaticSafePos(leader, urgentPositions);
	if (urgentPositions.empty()) {
		FallbackBasePos();
		return;
	}

	const AIFloat3& startPos = leader->GetPos(circuit->GetLastFrame());
	const float pathRange = DEFAULT_SLACK * 4;

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathMultiQuery(
			leader, circuit->GetThreatMap(),
			startPos, pathRange, urgentPositions);
	pathQueries[leader] = query;

	pathfinder->RunQuery(circuit->GetScheduler().get(), query, [this](const IPathQuery* query) {
		this->ApplyStaticSafe(static_cast<const CQueryPathMulti*>(query));
	});
}

void CAntiHeavyTask::ApplyStaticSafe(const CQueryPathMulti* query)
{
	pPath = query->GetPathInfo();

	if (!pPath->posPath.empty()) {
		position = pPath->posPath.back();
		ActivePath();
	} else {
		FallbackBasePos();
	}
}

void CAntiHeavyTask::FallbackBasePos()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CSetupManager* setupMgr = circuit->GetSetupManager();

	position = setupMgr->GetBasePos();

	const AIFloat3& startPos = leader->GetPos(circuit->GetLastFrame());
	const float pathRange = DEFAULT_SLACK * 4;

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathSingleQuery(
			leader, circuit->GetThreatMap(),
			startPos, position, pathRange);
	pathQueries[leader] = query;

	pathfinder->RunQuery(circuit->GetScheduler().get(), query, [this](const IPathQuery* query) {
		this->ApplyBasePos(static_cast<const CQueryPathSingle*>(query));
	});
}

void CAntiHeavyTask::ApplyBasePos(const CQueryPathSingle* query)
{
	pPath = query->GetPathInfo();

	if (!pPath->posPath.empty()) {
		position = pPath->posPath.back();
		ActivePath();
	} else {
		FallbackCommPos();
	}
}

void CAntiHeavyTask::FallbackCommPos()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CCircuitUnit* commander = circuit->GetSetupManager()->GetCommander();
	if ((commander != nullptr) &&
		circuit->GetTerrainManager()->CanMoveToPos(leader->GetArea(), commander->GetPos(frame)))
	{
		for (CCircuitUnit* unit : units) {
			unit->Guard(commander, frame + FRAMES_PER_SEC * 60);
			unit->GetTravelAct()->StateWait();
		}
		return;
	}

	position = circuit->GetSetupManager()->GetBasePos();
	for (CCircuitUnit* unit : units) {
		TRY_UNIT(circuit, unit,
			unit->CmdFightTo(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		)
		unit->GetTravelAct()->StateWait();
	}
}

} // namespace circuit
