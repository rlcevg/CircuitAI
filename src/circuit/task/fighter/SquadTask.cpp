/*
 * SquadTask.cpp
 *
 *  Created on: Jan 23, 2016
 *      Author: rlcevg
 */

#include "task/fighter/SquadTask.h"
#include "task/TaskManager.h"
#include "module/MilitaryManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/PathFinder.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

ISquadTask::ISquadTask(ITaskManager* mgr, FightType type)
		: IFighterTask(mgr, type)
		, lowestRange(std::numeric_limits<float>::max())
		, highestRange(.0f)
		, lowestSpeed(std::numeric_limits<float>::max())
		, highestSpeed(.0f)
		, leader(nullptr)
		, isRegroup(false)
		, isAttack(false)
{
}

ISquadTask::~ISquadTask()
{
}

void ISquadTask::AssignTo(CCircuitUnit* unit)
{
	IFighterTask::AssignTo(unit);

	if (leader == nullptr) {
		lowestRange  = unit->GetCircuitDef()->GetMaxRange();
		highestRange = unit->GetCircuitDef()->GetMaxRange();
		lowestSpeed  = unit->GetCircuitDef()->GetSpeed();
		highestSpeed = unit->GetCircuitDef()->GetSpeed();
		leader = unit;
	} else {
		lowestRange  = std::min(lowestRange,  unit->GetCircuitDef()->GetMaxRange());
		highestRange = std::max(highestRange, unit->GetCircuitDef()->GetMaxRange());
		lowestSpeed  = std::min(lowestSpeed,  unit->GetCircuitDef()->GetSpeed());
		highestSpeed = std::max(highestSpeed, unit->GetCircuitDef()->GetSpeed());
		if (leader->GetArea() == nullptr) {
			leader = unit;
		} else if ((unit->GetArea() != nullptr) && (unit->GetArea()->percentOfMap < leader->GetArea()->percentOfMap)) {
			leader = unit;
		}
	}
}

void ISquadTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		leader = nullptr;
		lowestRange = lowestSpeed = std::numeric_limits<float>::max();
		highestRange = highestSpeed = .0f;
		return;
	}

	FindLeader();
}

void ISquadTask::Merge(const std::set<CCircuitUnit*>& rookies, float power)
{
	units.insert(rookies.begin(), rookies.end());
	for (CCircuitUnit* unit : rookies) {
		unit->SetTask(this);
	}
	attackPower += power;

	FindLeader();
}

void ISquadTask::FindLeader()
{
	auto it = units.begin();
	leader = *it;
	lowestRange = highestRange = leader->GetCircuitDef()->GetMaxRange();
	lowestSpeed = highestSpeed = leader->GetCircuitDef()->GetSpeed();
	while (++it != units.end()) {
		CCircuitUnit* ass = *it;
		lowestRange  = std::min(lowestRange,  ass->GetCircuitDef()->GetMaxRange());
		highestRange = std::max(highestRange, ass->GetCircuitDef()->GetMaxRange());
		lowestSpeed  = std::min(lowestSpeed,  ass->GetCircuitDef()->GetSpeed());
		highestSpeed = std::max(highestSpeed, ass->GetCircuitDef()->GetSpeed());
		if (leader->GetArea() == nullptr) {
			leader = ass;
		} else if ((ass->GetArea() != nullptr) && (ass->GetArea()->percentOfMap < leader->GetArea()->percentOfMap)) {
			leader = ass;
		}
	}
}

ISquadTask* ISquadTask::GetMergeTask() const
{
	const IFighterTask* task = nullptr;
	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();

	AIFloat3 pos = leader->GetPos(frame);
	STerrainMapArea* area = leader->GetArea();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CPathFinder* pathfinder = circuit->GetPathfinder();
	terrainManager->CorrectPosition(pos);
	pathfinder->SetMapData(leader, circuit->GetThreatMap(), frame);
	const int distance = pathfinder->GetSquareSize();
	float metric = std::numeric_limits<float>::max();

	const std::set<IFighterTask*>& tasks = static_cast<CMilitaryManager*>(manager)->GetTasks(fightType);
	for (const IFighterTask* candidate : tasks) {
		if ((candidate == this) || !candidate->CanAssignTo(leader)) {
			continue;
		}

		// Check time-distance to target
		float distCost;

		const AIFloat3& tp = candidate->GetPosition();
		AIFloat3 taskPos = utils::is_valid(tp) ? tp : pos;

		if (!terrainManager->CanMoveToPos(area, taskPos)) {  // ensure that path always exists
			continue;
		}

		distCost = std::max(pathfinder->PathCost(pos, taskPos, distance), THREAT_BASE);

		if ((distCost < metric) && (distCost < MAX_TRAVEL_SEC * THREAT_BASE)) {
			task = candidate;
			metric = distCost;
		}
	}

	return static_cast<ISquadTask*>(const_cast<IFighterTask*>(task));
}

bool ISquadTask::IsMustRegroup()
{
	if (isAttack || (updCount % 16 != 0)) {
		return false;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	static std::vector<CCircuitUnit*> validUnits;  // NOTE: micro-opt
//	validUnits.reserve(units.size());
	CTerrainManager* terrainManager = circuit->GetTerrainManager();;
	for (CCircuitUnit* unit : units) {
		AIFloat3 pos = unit->GetPos(frame);
		terrainManager->CorrectPosition(pos);
		if (terrainManager->CanMoveToPos(unit->GetArea(), pos)) {
			validUnits.push_back(unit);
		}
	}
	if (validUnits.empty()) {
		return isRegroup = false;
	}
	const AIFloat3& groupPos = leader->GetPos(frame);

	isRegroup = false;
	const float sqMaxDist = SQUARE(std::max<float>(SQUARE_SIZE * 4 * validUnits.size(), highestRange));
	for (CCircuitUnit* unit : units) {
		const float sqDist = groupPos.SqDistance2D(unit->GetPos(frame));
		if (sqDist > sqMaxDist) {
			isRegroup = true;
			break;
		}
	}

	validUnits.clear();
	return isRegroup;
}

} // namespace circuit
