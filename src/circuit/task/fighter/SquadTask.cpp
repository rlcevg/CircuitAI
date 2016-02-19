/*
 * SquadTask.cpp
 *
 *  Created on: Jan 23, 2016
 *      Author: rlcevg
 */

#include "task/fighter/SquadTask.h"
#include "task/TaskManager.h"
#include "terrain/TerrainManager.h"
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

bool ISquadTask::IsMustRegroup()
{
	if (isAttack || (updCount % 16 != 0)) {
		return false;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	std::vector<CCircuitUnit*> validUnits;
	validUnits.reserve(units.size());
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

	return isRegroup;
}

} // namespace circuit
