/*
 * RaidTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/RaidTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/PathFinder.h"
#include "unit/action/MoveAction.h"
#include "unit/action/FightAction.h"
#include "unit/enemy/EnemyUnit.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CRaidTask::CRaidTask(ITaskManager* mgr, float maxPower, float powerMod)
		: ISquadTask(mgr, FightType::RAID, powerMod)
		, maxPower(maxPower)
{
	CCircuitAI* circuit = manager->GetCircuit();
	float x = rand() % circuit->GetTerrainManager()->GetTerrainWidth();
	float z = rand() % circuit->GetTerrainManager()->GetTerrainHeight();
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
}

CRaidTask::~CRaidTask()
{
}

bool CRaidTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (!unit->GetCircuitDef()->IsRoleRaider() ||
		(unit->GetCircuitDef() != leader->GetCircuitDef()))
	{
		return false;
	}
	if (attackPower > maxPower) {
		return false;
	}
	const int frame = manager->GetCircuit()->GetLastFrame();
	if (leader->GetPos(frame).SqDistance2D(unit->GetPos(frame)) > SQUARE(1000.f)) {
		return false;
	}
	return true;
}

void CRaidTask::AssignTo(CCircuitUnit* unit)
{
	ISquadTask::AssignTo(unit);
	CCircuitDef* cdef = unit->GetCircuitDef();
	highestRange = std::max(highestRange, cdef->GetLosRadius());
	highestRange = std::max(highestRange, cdef->GetJumpRange());

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	ITravelAction* travelAction;
	if (cdef->IsAttrSiege()) {
		travelAction = new CFightAction(unit, squareSize);
	} else {
		travelAction = new CMoveAction(unit, squareSize);
	}
	unit->PushTravelAct(travelAction);
	travelAction->SetActive(false);
}

void CRaidTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);
	if (leader == nullptr) {
		manager->AbortTask(this);
	} else {
		highestRange = std::max(highestRange, leader->GetCircuitDef()->GetLosRadius());
		highestRange = std::max(highestRange, leader->GetCircuitDef()->GetJumpRange());
	}
}

void CRaidTask::Start(CCircuitUnit* unit)
{
	if ((State::REGROUP == state) || (State::ENGAGE == state)) {
		return;
	}
	if (!pPath->posPath.empty()) {
		unit->GetTravelAct()->SetPath(pPath);
		unit->GetTravelAct()->SetActive(true);
	}
}

void CRaidTask::Update()
{
	++updCount;

	/*
	 * Merge tasks if possible
	 */
	if (updCount % 32 == 1) {
		ISquadTask* task = GetMergeTask();
		if (task != nullptr) {
			task->Merge(this);
			units.clear();
			manager->AbortTask(this);
			return;
		}
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
				const AIFloat3& pos = utils::get_radial_pos(groupPos, SQUARE_SIZE * 8);
				TRY_UNIT(circuit, unit,
					unit->GetUnit()->MoveTo(groupPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame);
					unit->GetUnit()->PatrolTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, frame);
				)

				unit->GetTravelAct()->SetActive(false);
			}
		}
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	bool isExecute = (updCount % 2 == 0) && (frame >= lastTouched + FRAMES_PER_SEC);
	if (!isExecute) {
		for (CCircuitUnit* unit : units) {
			isExecute |= unit->IsForceExecute(frame);
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
	FindTarget();

	state = State::ROAM;
	if (target != nullptr) {
		state = State::ENGAGE;
		position = target->GetPos();
		if (leader->GetCircuitDef()->IsAbleToFly()) {
			if (target->GetUnit()->IsCloaked()) {
				for (CCircuitUnit* unit : units) {
					const AIFloat3& pos = target->GetPos();
					TRY_UNIT(circuit, unit,
						unit->GetUnit()->ExecuteCustomCommand(CMD_ATTACK_GROUND, {pos.x, pos.y, pos.z}, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
					)

					unit->GetTravelAct()->SetActive(false);
				}
			} else {
				for (CCircuitUnit* unit : units) {
					TRY_UNIT(circuit, unit,
						unit->GetUnit()->Attack(target->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
//						unit->GetUnit()->ExecuteCustomCommand(CMD_UNIT_SET_TARGET, {(float)target->GetId()});
					)

					unit->GetTravelAct()->SetActive(false);
				}
			}
		} else {
			Attack(frame);
		}
		return;
	} else if (!pPath->posPath.empty()) {
		position = pPath->posPath.back();
		ActivePath();
		return;
	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& pos = leader->GetPos(frame);
	const AIFloat3& threatPos = leader->GetTravelAct()->IsActive() ? position : pos;
	if (attackPower * powerMod <= threatMap->GetThreatAt(leader, threatPos)) {
		position = circuit->GetMilitaryManager()->GetRaidPosition(leader);
	}

	if (!utils::is_valid(position)) {
		float x = rand() % terrainManager->GetTerrainWidth();
		float z = rand() % terrainManager->GetTerrainHeight();
		position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
		position = terrainManager->GetMovePosition(leader->GetArea(), position);
	}
	AIFloat3 startPos = pos;
	AIFloat3 endPos = position;

	CPathFinder* pathfinder = circuit->GetPathfinder();
	pathfinder->SetMapData(leader, threatMap, frame);
	pathfinder->MakePath(*pPath, startPos, endPos, pathfinder->GetSquareSize());

	if (pPath->path.size() > 2) {
//		position = path.back();
		ActivePath();
		return;
	}

	for (CCircuitUnit* unit : units) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		)
		unit->GetTravelAct()->SetActive(false);
	}
}

void CRaidTask::OnUnitIdle(CCircuitUnit* unit)
{
	ISquadTask::OnUnitIdle(unit);
	if (units.empty()) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	const float maxDist = std::max<float>(lowestRange, circuit->GetPathfinder()->GetSquareSize());
	if (position.SqDistance2D(leader->GetPos(circuit->GetLastFrame())) < SQUARE(maxDist)) {
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		float x = rand() % terrainManager->GetTerrainWidth();
		float z = rand() % terrainManager->GetTerrainHeight();
		position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
		position = terrainManager->GetMovePosition(leader->GetArea(), position);
	}

	if (units.find(unit) != units.end()) {
		Start(unit);  // NOTE: Not sure if it has effect
	}
}

void CRaidTask::FindTarget()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CMap* map = circuit->GetMap();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	STerrainMapArea* area = leader->GetArea();
	CCircuitDef* cdef = leader->GetCircuitDef();
	const AIFloat3& pos = leader->GetPos(circuit->GetLastFrame());
	const bool notAW = !cdef->HasAntiWater();
	const bool notAA = !cdef->HasAntiAir();
	const float speed = SQUARE(highestSpeed * 0.9f / FRAMES_PER_SEC);
	const float maxPower = attackPower * powerMod;
	const float weaponRange = cdef->GetMaxRange();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float range = std::max(leader->GetUnit()->GetMaxRange() + threatMap->GetSquareSize() * 2,
								 cdef->GetLosRadius());
	float minSqDist = SQUARE(range);
	float maxThreat = 0.f;
	float minPower = maxPower;

	SetTarget(nullptr);  // make adequate enemy->GetTasks().size()
	CEnemyInfo* bestTarget = nullptr;
	CEnemyInfo* worstTarget = nullptr;
	static F3Vec enemyPositions;  // NOTE: micro-opt
	threatMap->SetThreatType(leader);
	const CCircuitAI::EnemyInfos& enemies = circuit->GetEnemyInfos();
	for (auto& kv : enemies) {
		CEnemyInfo* enemy = kv.second;
		if (enemy->IsHidden() || (enemy->GetTasks().size() > 2)) {
			continue;
		}
		const AIFloat3& ePos = enemy->GetPos();
		const float power = threatMap->GetThreatAt(ePos);
		if ((maxPower <= power) ||
			!terrainManager->CanMoveToPos(area, ePos) ||
			(enemy->GetVel().SqLength2D() >= speed))
		{
			continue;
		}

		int targetCat;
		float defThreat;
		bool isBuilder;
		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			targetCat = edef->GetCategory();
			if (((targetCat & canTargetCat) == 0) ||
				(edef->IsAbleToFly() && notAA))
			{
				continue;
			}
			float elevation = map->GetElevationAt(ePos.x, ePos.z);
			if ((notAW && !edef->IsYTargetable(elevation, ePos.y)) ||
				(ePos.y - elevation > weaponRange))
			{
				continue;
			}
			defThreat = edef->GetPower();
			isBuilder = edef->IsEnemyRoleAny(CCircuitDef::RoleMask::BUILDER | CCircuitDef::RoleMask::COMM);
		} else {
			if (notAW && (ePos.y < -SQUARE_SIZE * 5)) {
				continue;
			}
			targetCat = UNKNOWN_CATEGORY;
			defThreat = enemy->GetThreat();
			isBuilder = false;
		}

		float sqDist = pos.SqDistance2D(ePos);
		if ((minPower > power) && (minSqDist > sqDist)) {
			if (enemy->IsInRadarOrLOS()) {
				if (((targetCat & noChaseCat) == 0) && !enemy->IsBeingBuilt()) {
					if (isBuilder) {
						bestTarget = enemy;
						minSqDist = sqDist;
						maxThreat = std::numeric_limits<float>::max();
					} else if (maxThreat <= defThreat) {
						bestTarget = enemy;
						minSqDist = sqDist;
						maxThreat = defThreat;
					}
					minPower = power;
				} else if (bestTarget == nullptr) {
					worstTarget = enemy;
				}
			}
			continue;
		}
//		if (sqDist < SQUARE(2000.f)) {  // maxSqDist
			enemyPositions.push_back(ePos);
//		}
	}
	if (bestTarget == nullptr) {
		bestTarget = worstTarget;
	}

	if (bestTarget != nullptr) {
		SetTarget(bestTarget);
		enemyPositions.clear();
		pPath->Clear();
		return;
	}
	if (enemyPositions.empty()) {
		pPath->Clear();
		return;
	}

	AIFloat3 startPos = pos;
	const float pathRange = std::max(std::min(weaponRange, cdef->GetLosRadius()), (float)threatMap->GetSquareSize());
	CPathFinder* pathfinder = circuit->GetPathfinder();
	pathfinder->SetMapData(leader, threatMap, circuit->GetLastFrame());
	pathfinder->FindBestPath(*pPath, startPos, pathRange, enemyPositions, attackPower);
	enemyPositions.clear();
}

} // namespace circuit
