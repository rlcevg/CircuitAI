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
#include "unit/action/TravelAction.h"
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
		, groupPos(-RgtVector)
		, pPath(std::make_shared<F3Vec>())
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

	leader = *units.begin();
	lowestRange = highestRange = leader->GetCircuitDef()->GetMaxRange();
	lowestSpeed = highestSpeed = leader->GetCircuitDef()->GetSpeed();
	FindLeader(++units.begin(), units.end());
}

void ISquadTask::Merge(const std::set<CCircuitUnit*>& rookies, float power)
{
	bool isActive = static_cast<ITravelAction*>(leader->End())->IsActive();
	for (CCircuitUnit* unit : rookies) {
		unit->SetTask(this);

		ITravelAction* travelAction = static_cast<ITravelAction*>(unit->End());
		travelAction->SetPath(pPath);
		travelAction->SetActive(isActive);
	}
	units.insert(rookies.begin(), rookies.end());
	attackPower += power;

	FindLeader(rookies.begin(), rookies.end());
}

const AIFloat3& ISquadTask::GetLeaderPos(int frame) const
{
	return (leader != nullptr) ? leader->GetPos(frame) : GetPosition();
}

void ISquadTask::FindLeader(decltype(units)::iterator itBegin, decltype(units)::iterator itEnd)
{
	for (; itBegin != itEnd; ++itBegin) {
		CCircuitUnit* ass = *itBegin;
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
	const ISquadTask* task = nullptr;
	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();

	AIFloat3 pos = leader->GetPos(frame);
	STerrainMapArea* area = leader->GetArea();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CPathFinder* pathfinder = circuit->GetPathfinder();
	terrainManager->CorrectPosition(pos);
	pathfinder->SetMapData(leader, circuit->GetThreatMap(), frame);
	const float maxSpeed = lowestSpeed / pathfinder->GetSquareSize() * THREAT_BASE;
	const float maxDistCost = MAX_TRAVEL_SEC * (maxSpeed * FRAMES_PER_SEC);
	const int distance = pathfinder->GetSquareSize();
	float metric = std::numeric_limits<float>::max();

	const std::set<IFighterTask*>& tasks = static_cast<CMilitaryManager*>(manager)->GetTasks(fightType);
	for (const IFighterTask* candidate : tasks) {
		if ((candidate == this) ||
			(candidate->GetAttackPower() < attackPower) ||
			!candidate->CanAssignTo(leader))
		{
			continue;
		}
		const ISquadTask* candy = static_cast<const ISquadTask*>(candidate);

		// Check time-distance to target
		float distCost;

		const AIFloat3& tp = candy->GetLeaderPos(frame);
		AIFloat3 taskPos = utils::is_valid(tp) ? tp : pos;

		if (!terrainManager->CanMoveToPos(area, taskPos)) {  // ensure that path always exists
			continue;
		}

		distCost = std::max(pathfinder->PathCost(pos, taskPos, distance), THREAT_BASE);

		if ((distCost < metric) && (distCost < maxDistCost)) {
			task = candy;
			metric = distCost;
		}
	}

	return const_cast<ISquadTask*>(task);
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
	if (!isRegroup) {
		groupPos = leader->GetPos(frame);
	}

	isRegroup = false;
	const float sqMaxDist = SQUARE(std::max<float>(SQUARE_SIZE * 8 * validUnits.size(), highestRange));
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
