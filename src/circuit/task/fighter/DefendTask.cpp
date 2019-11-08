/*
 * DefendTask.cpp
 *
 *  Created on: Feb 12, 2016
 *      Author: rlcevg
 */

#include "task/fighter/DefendTask.h"
#include "task/TaskManager.h"
#include "module/MilitaryManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/InfluenceMap.h"
#include "terrain/ThreatMap.h"
#include "terrain/PathFinder.h"
#include "unit/action/FightAction.h"
#include "unit/action/MoveAction.h"
#include "unit/action/SupportAction.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "AISCommands.h"

namespace circuit {

using namespace springai;

CDefendTask::CDefendTask(ITaskManager* mgr, const AIFloat3& position, float radius,
						 FightType check, FightType promote, float maxPower, float powerMod)
		: ISquadTask(mgr, FightType::DEFEND, powerMod)
		, radius(radius)
		, check(check)
		, promote(promote)
		, maxPower(maxPower * powerMod)
{
	this->position = position;
}

CDefendTask::~CDefendTask()
{
}

bool CDefendTask::CanAssignTo(CCircuitUnit* unit) const
{
	return (attackPower < maxPower) && (static_cast<CDefendTask*>(unit->GetTask())->GetPromote() == promote);
}

void CDefendTask::AssignTo(CCircuitUnit* unit)
{
	ISquadTask::AssignTo(unit);
	CCircuitDef* cdef = unit->GetCircuitDef();
	highestRange = std::max(highestRange, cdef->GetLosRadius());

	if (cdef->IsRoleSupport()) {
		unit->PushBack(new CSupportAction(unit));
	}

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	ITravelAction* travelAction;
	if (cdef->IsAttrSiege()) {
		travelAction = new CFightAction(unit, squareSize);
	} else/* if (cdef->IsAttrMelee())*/ {
		travelAction = new CMoveAction(unit, squareSize);
	}
	unit->PushTravelAct(travelAction);
	travelAction->SetActive(false);
}

void CDefendTask::RemoveAssignee(CCircuitUnit* unit)
{
	ISquadTask::RemoveAssignee(unit);
	if (leader == nullptr) {
		manager->AbortTask(this);
	}
}

void CDefendTask::Start(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	AIFloat3 pos = utils::get_radial_pos(position, SQUARE_SIZE * 32);
	CTerrainManager::CorrectPosition(pos);
	pos = terrainManager->FindBuildSite(unit->GetCircuitDef(), pos, 300.0f, UNIT_COMMAND_BUILD_NO_FACING);

	TRY_UNIT(circuit, unit,
		unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
		unit->GetUnit()->ExecuteCustomCommand(CMD_WANTED_SPEED, {NO_SPEED_LIMIT});
	)
}

void CDefendTask::Update()
{
	++updCount;

	/*
	 * Merge tasks if possible
	 */
	if (updCount % 32 == 1) {
		CMilitaryManager* militaryManager = static_cast<CMilitaryManager*>(manager);
		if ((attackPower >= maxPower) || !militaryManager->GetTasks(check).empty()) {
			IFighterTask* task = militaryManager->EnqueueTask(promote);
			decltype(units) tmpUnits = units;
			for (CCircuitUnit* unit : tmpUnits) {
				manager->AssignTask(unit, task);
			}
//			manager->DoneTask(this);  // NOTE: RemoveAssignee() will abort task
			return;
		}

		ISquadTask* task = GetMergeTask();
		if (task != nullptr) {
			task->Merge(this);
			units.clear();
			manager->AbortTask(this);
			return;
		}
	}

	/*
	 * No regroup
	 */
	bool isExecute = (updCount % 16 == 2);
	if (!isExecute) {
		for (CCircuitUnit* unit : units) {
			isExecute |= unit->IsForceExecute();
		}
		if (!isExecute) {
			return;
		}
	} else {
		ISquadTask::Update();
		if (leader == nullptr) {  // task aborted
			return;
		}
	}

	/*
	 * Update target
	 */
	FindTarget();

	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	state = State::ROAM;
	if (target != nullptr) {
		const float sqRange = SQUARE(highestRange);
		for (CCircuitUnit* unit : units) {
			if (position.SqDistance2D(unit->GetPos(frame)) < sqRange) {
				state = State::ENGAGE;
				break;
			}
		}
		if (State::ENGAGE == state) {
			for (CCircuitUnit* unit : units) {
				if (unit->Blocker() != nullptr) {
					continue;  // Do not interrupt current action
				}

				unit->Attack(target->GetPos(), target, frame + FRAMES_PER_SEC * 60);

				unit->GetTravelAct()->SetActive(false);
			}
			return;
		}
	} else {
		AIFloat3 startPos = leader->GetPos(frame);
		CPathFinder* pathfinder = circuit->GetPathfinder();
		pathfinder->SetMapData(leader, circuit->GetThreatMap(), frame);
		pathfinder->PreferPath(pPath->path);
		circuit->GetMilitaryManager()->FindFrontPos(*pPath, startPos, leader->GetArea(), DEFAULT_SLACK * 4);
		pathfinder->UnpreferPath();

		if (!pPath->path.empty()) {
			if (pPath->path.size() > 2) {
				ActivePath();
			}
			return;
		}
	}
	if (pPath->posPath.empty()) {  // should never happen
		for (CCircuitUnit* unit : units) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
				unit->GetUnit()->ExecuteCustomCommand(CMD_WANTED_SPEED, {lowestSpeed});
			)

			unit->GetTravelAct()->SetActive(false);
		}
	} else {
		ActivePath(lowestSpeed);
	}
}

void CDefendTask::Merge(ISquadTask* task)
{
	const std::set<CCircuitUnit*>& rookies = task->GetAssignees();
	for (CCircuitUnit* unit : rookies) {
		unit->SetTask(this);
	}
	units.insert(rookies.begin(), rookies.end());
	maxPower = std::max(maxPower, static_cast<CDefendTask*>(task)->GetMaxPower());
	attackPower += task->GetAttackPower();
	const std::set<CCircuitUnit*>& sh = task->GetShields();
	shields.insert(sh.begin(), sh.end());
}

void CDefendTask::FindTarget()
{
	CCircuitAI* circuit = manager->GetCircuit();
	Map* map = circuit->GetMap();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	CInfluenceMap* inflMap = circuit->GetInflMap();
	const AIFloat3& pos = leader->GetPos(circuit->GetLastFrame());
	STerrainMapArea* area = leader->GetArea();
	CCircuitDef* cdef = leader->GetCircuitDef();
	const bool notAW = !cdef->HasAntiWater();
	const bool notAA = !cdef->HasAntiAir();
	const float maxPower = attackPower * powerMod;
	const float weaponRange = cdef->GetMaxRange();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();

	CEnemyUnit* bestTarget = nullptr;
	float minSqDist = std::numeric_limits<float>::max();

	SetTarget(nullptr);  // make adequate enemy->GetTasks().size()
	static F3Vec enemyPositions;  // NOTE: micro-opt
	threatMap->SetThreatType(leader);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() || (enemy->GetTasks().size() > 2)) {
			continue;
		}
		const AIFloat3& ePos = enemy->GetPos();
		if ((maxPower <= threatMap->GetThreatAt(ePos)) || (inflMap->GetInfluenceAt(ePos) < INFL_BASE) ||
			!terrainManager->CanMoveToPos(area, ePos))
		{
			continue;
		}

		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			if (((edef->GetCategory() & canTargetCat) == 0) || ((edef->GetCategory() & noChaseCat) != 0) ||
				(edef->IsAbleToFly() && notAA))
			{
				continue;
			}
			float elevation = map->GetElevationAt(ePos.x, ePos.z);
			if ((notAW && !edef->IsYTargetable(elevation, ePos.y)) ||
				(ePos.y - elevation > weaponRange) ||
				enemy->IsBeingBuilt())
			{
				continue;
			}
		} else {
			if (notAW && (ePos.y < -SQUARE_SIZE * 5)) {
				continue;
			}
		}

		float sqDist = pos.SqDistance2D(ePos);
		if (minSqDist > sqDist) {
			minSqDist = sqDist;
			bestTarget = enemy;
		}
		enemyPositions.push_back(ePos);
	}

	if (bestTarget != nullptr) {
		SetTarget(bestTarget);
		position = target->GetPos();
	}
	if (enemyPositions.empty()) {
		pPath->Clear();
		return;
	}
	AIFloat3 startPos = pos;

	const float range = std::max(highestRange - threatMap->GetSquareSize() * 2.f, threatMap->GetSquareSize() * 2.f);
	CPathFinder* pathfinder = circuit->GetPathfinder();
	pathfinder->SetMapData(leader, threatMap, circuit->GetLastFrame());
	pathfinder->PreferPath(pPath->path);
	pathfinder->FindBestPath(*pPath, startPos, range, enemyPositions);
	pathfinder->UnpreferPath();
	enemyPositions.clear();
}

} // namespace circuit
