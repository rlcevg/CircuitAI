/*
 * SupportTask.cpp
 *
 *  Created on: Jul 3, 2016
 *      Author: rlcevg
 */

#include "task/fighter/SupportTask.h"
#include "task/fighter/SquadTask.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/PathFinder.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CSupportTask::CSupportTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::SUPPORT)
		, updCount(0)
{
	const AIFloat3& pos = manager->GetCircuit()->GetSetupManager()->GetBasePos();
	position = utils::get_radial_pos(pos, SQUARE_SIZE * 32);
}

CSupportTask::~CSupportTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CSupportTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CSupportTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	AIFloat3 pos = terrainManager->FindBuildSite(unit->GetCircuitDef(), position, 300.0f, UNIT_COMMAND_BUILD_NO_FACING);

	TRY_UNIT(circuit, unit,
		unit->GetUnit()->MoveTo(pos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
		unit->GetUnit()->SetWantedMaxSpeed(MAX_UNIT_SPEED);
	)
}

void CSupportTask::Update()
{
	if (updCount++ % 8 != 0) {
		return;
	}

	const std::set<IFighterTask*>& tasks = static_cast<CMilitaryManager*>(manager)->GetTasks(IFighterTask::FightType::ATTACK);
	if (tasks.empty()) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	F3Vec ourPositions;
	ourPositions.reserve(tasks.size());
	for (IFighterTask* candy : tasks) {
		ourPositions.push_back(static_cast<ISquadTask*>(candy)->GetLeaderPos(frame));
	}

	F3Vec path;
	CCircuitUnit* unit = *units.begin();
	AIFloat3 startPos = unit->GetPos(frame);
	CPathFinder* pathfinder = circuit->GetPathfinder();
	pathfinder->SetMapData(unit, circuit->GetThreatMap(), frame);
	pathfinder->FindBestPath(path, startPos, pathfinder->GetSquareSize(), ourPositions);
	if (path.empty()) {
		return;
	}

	IFighterTask* task = *tasks.begin();
	const AIFloat3& endPos = path.back();
	float minSqDist = std::numeric_limits<float>::max();
	for (IFighterTask* candy : tasks) {
		float sqDist = endPos.SqDistance2D(static_cast<ISquadTask*>(candy)->GetLeaderPos(frame));
		if (minSqDist > sqDist) {
			minSqDist = sqDist;
			task = candy;
		}
	}

	if (startPos.SqDistance2D(endPos) < SQUARE(1000.f)) {
		manager->AssignTask(unit, task);
//		manager->DoneTask(this);  // NOTE: RemoveAssignee will abort task
	} else {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->MoveTo(endPos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
		)
	}
}

} // namespace circuit
