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
#include "terrain/path/PathFinder.h"
#include "terrain/path/QueryPathMulti.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CSupportTask::CSupportTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::SUPPORT, 1.f)
{
	const AIFloat3& pos = manager->GetCircuit()->GetSetupManager()->GetBasePos();
	position = utils::get_radial_pos(pos, SQUARE_SIZE * 32);
}

CSupportTask::~CSupportTask()
{
}

void CSupportTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CSupportTask::Start(CCircuitUnit* unit)
{
	if (State::DISENGAGE == state) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	AIFloat3 pos = position;
	CTerrainManager::CorrectPosition(pos);
	AIFloat3 freePos = terrainMgr->FindBuildSite(unit->GetCircuitDef(), pos, 300.0f, UNIT_NO_FACING, true);
//	AIFloat3 freePos = terrainMgr->FindSpringBuildSite(unit->GetCircuitDef(), pos, 300.0f, UNIT_NO_FACING);
	pos = utils::is_valid(freePos) ? freePos : pos;

	TRY_UNIT(circuit, unit,
		unit->CmdFightTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
		unit->CmdWantedSpeed(NO_SPEED_LIMIT);
	)
	state = State::DISENGAGE;  // Wait
}

void CSupportTask::Update()
{
	if (updCount++ % 8 != 0) {
		return;
	}

	CCircuitUnit* unit = *units.begin();
	const std::set<IFighterTask*>& tasksA = static_cast<CMilitaryManager*>(manager)->GetTasks(IFighterTask::FightType::ATTACK);
	const std::set<IFighterTask*>& tasksD = static_cast<CMilitaryManager*>(manager)->GetTasks(IFighterTask::FightType::DEFEND);
	const std::set<IFighterTask*>& tasks = tasksA.empty() ? tasksD : tasksA;
	if (tasks.empty()) {
		Start(unit);
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	urgentPositions.clear();
//	urgentPositions.reserve(tasks.size());
	for (IFighterTask* candy : tasks) {
		ISquadTask* task = static_cast<ISquadTask*>(candy);
		CCircuitUnit* leader = task->GetLeader();
		if (leader == nullptr) {
			continue;
		}
		const AIFloat3& pos = leader->GetPos(frame);
		if (!terrainMgr->CanMoveToPos(unit->GetArea(), pos)) {
			continue;
		}
		if (((unit->GetCircuitDef()->IsAmphibious() || unit->GetCircuitDef()->IsSurfer())
				&& (leader->GetCircuitDef()->IsAbleToDive() || leader->GetCircuitDef()->IsSurfer()))
			|| (leader->GetCircuitDef()->IsSubmarine() && unit->GetCircuitDef()->IsSubmarine())
			|| (leader->GetCircuitDef()->IsAbleToFly() && unit->GetCircuitDef()->IsAbleToFly())
			|| (leader->GetCircuitDef()->IsLander() && unit->GetCircuitDef()->IsLander())
			|| (leader->GetCircuitDef()->IsFloater() && unit->GetCircuitDef()->IsFloater()))
		{
			urgentPositions.push_back(pos);
		}
	}
	if (urgentPositions.empty()) {
		Start(unit);
		return;
	}

	if (!IsQueryReady(unit)) {
		return;
	}

	CPathFinder* pathfinder = circuit->GetPathfinder();
	const AIFloat3& startPos = unit->GetPos(frame);
	const float range = pathfinder->GetSquareSize();

	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathMultiQuery(
			unit, circuit->GetThreatMap(),
			startPos, range, urgentPositions, nullptr, false, std::numeric_limits<float>::max(), true);
	pathQueries[unit] = query;

	pathfinder->RunQuery(query, [this](const IPathQuery* query) {
		this->ApplyPath(static_cast<const CQueryPathMulti*>(query));
	});
}

void CSupportTask::ApplyPath(const CQueryPathMulti* query)
{
	const std::shared_ptr<CPathInfo>& pPath = query->GetPathInfo();
	CCircuitUnit* unit = query->GetUnit();

	if (pPath->posPath.empty()) {
		Start(unit);
		return;
	}

	const std::set<IFighterTask*>& tasksA = static_cast<CMilitaryManager*>(manager)->GetTasks(IFighterTask::FightType::ATTACK);
	const std::set<IFighterTask*>& tasksD = static_cast<CMilitaryManager*>(manager)->GetTasks(IFighterTask::FightType::DEFEND);
	const std::set<IFighterTask*>& tasks = tasksA.empty() ? tasksD : tasksA;
	if (tasks.empty()) {
		Start(unit);
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& startPos = unit->GetPos(frame);
	const AIFloat3& endPos = pPath->posPath.back();
	if (startPos.SqDistance2D(endPos) < SQUARE(1000.f)) {
		IFighterTask* task = *tasks.begin();
		float minSqDist = std::numeric_limits<float>::max();
		for (IFighterTask* candy : tasks) {
			float sqDist = endPos.SqDistance2D(static_cast<ISquadTask*>(candy)->GetLeaderPos(frame));
			if (minSqDist > sqDist) {
				minSqDist = sqDist;
				task = candy;
			}
		}
		manager->AssignTask(unit, task);
//		manager->DoneTask(this);  // NOTE: RemoveAssignee will abort task
	} else {
		TRY_UNIT(circuit, unit,
			unit->CmdFightTo(endPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		)
		state = State::ROAM;  // Not wait
	}
}

} // namespace circuit
