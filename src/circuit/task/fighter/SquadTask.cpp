/*
 * SquadTask.cpp
 *
 *  Created on: Jan 23, 2016
 *      Author: rlcevg
 */

#include "task/fighter/SquadTask.h"
#include "task/TaskManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

ISquadTask::ISquadTask(ITaskManager* mgr, FightType type)
		: IFighterTask(mgr, type)
		, lowestRange(std::numeric_limits<float>::max())
		, highestRange(.0f)
		, lowestSpeed(std::numeric_limits<float>::max())
		, highestSpeed(.0f)
		, leader(nullptr)
		, minPower(.0f)
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

	minPower += unit->GetCircuitDef()->GetPower() / 8;

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

	if (attackPower < minPower) {
		manager->AbortTask(this);
		return;
	}

	lowestRange  = std::numeric_limits<float>::max();
	highestRange = .0f;
	lowestSpeed  = std::numeric_limits<float>::max();
	highestSpeed = .0f;
	leader = *units.begin();
	for (CCircuitUnit* ass : units) {
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

bool ISquadTask::IsRegroup()
{
	if (isAttack || (updCount % 16 != 0)) {
		return isRegroup;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	std::vector<CCircuitUnit*> veterans;
	veterans.reserve(units.size());
	CTerrainManager* terrainManager = circuit->GetTerrainManager();;
	AIFloat3 groupPos(ZeroVector);
	for (CCircuitUnit* unit : units) {
		AIFloat3 pos = unit->GetPos(frame);
		terrainManager->CorrectPosition(pos);
		if (terrainManager->CanMoveToPos(unit->GetArea(), pos)) {
			groupPos += pos;
			veterans.push_back(unit);
		}
	}
	if (veterans.empty()) {
		return isRegroup = false;
	}
	groupPos /= veterans.size();

	// find the unit closest to the center (since the actual center might be unreachable)
	float sqMinDist = std::numeric_limits<float>::max();
	float sqMaxDist = .0f;
	CCircuitUnit* closestUnit = *units.begin();
	for (CCircuitUnit* unit : veterans) {
		const float sqDist = groupPos.SqDistance2D(unit->GetPos(frame));
		if (sqDist < sqMinDist) {
			sqMinDist = sqDist;
			closestUnit = unit;
		}
		if (sqDist > sqMaxDist) {
			sqMaxDist = sqDist;
		}
	}
	groupPos = closestUnit->GetPos(frame);  // TODO: Why not use leader?

	// have to regroup?
	const float regroupDist = std::max<float>(SQUARE_SIZE * 4 * veterans.size(), highestRange);
	if (sqMaxDist > regroupDist * regroupDist) {
		isRegroup = true;
		for (CCircuitUnit* unit : units) {
			unit->GetUnit()->MoveTo(groupPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			unit->GetUnit()->SetWantedMaxSpeed(MAX_SPEED);
		}
	} else {
		isRegroup = false;
	}
	return isRegroup;
}

} // namespace circuit
