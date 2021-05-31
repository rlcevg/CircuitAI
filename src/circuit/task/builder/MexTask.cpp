/*
 * MexTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/MexTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/EconomyManager.h"
#include "module/BuilderManager.h"
#include "module/MilitaryManager.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CBMexTask::CBMexTask(ITaskManager* mgr, Priority priority,
					 CCircuitDef* buildDef, const AIFloat3& position,
					 float cost, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::MEX, cost, 0.f, timeout)
		, blockCount(0)
{
}

CBMexTask::~CBMexTask()
{
}

bool CBMexTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (!IBuilderTask::CanAssignTo(unit)) {
		return false;
	}
	CCircuitAI* circuit = manager->GetCircuit();
//	if (circuit->GetEconomyManager()->IsEnergyStalling()) {
//		return false;
//	}
	if (unit->GetCircuitDef()->IsAttacker()) {
		return true;
	}
	CMilitaryManager* militaryMgr = circuit->GetMilitaryManager();
	if ((militaryMgr->GetGuardTaskNum() == 0) || (circuit->GetLastFrame() > militaryMgr->GetGuardFrame())) {
		return true;
	}
	int cluster = circuit->GetMetalManager()->FindNearestCluster(GetPosition());
	if ((cluster < 0) || militaryMgr->HasDefence(cluster)) {
		return true;
	}
	IUnitTask* guard = militaryMgr->GetGuardTask(unit);
	return (guard != nullptr) && !guard->GetAssignees().empty();
}

void CBMexTask::Cancel()
{
	if ((target == nullptr) && utils::is_valid(buildPos)) {
		CCircuitAI* circuit = manager->GetCircuit();
		int index = circuit->GetMetalManager()->FindNearestSpot(buildPos);
		circuit->GetMetalManager()->SetOpenSpot(index, true);
		circuit->GetEconomyManager()->SetOpenSpot(index, true);
		SetBuildPos(-RgtVector);
	}
}

void CBMexTask::Execute(CCircuitUnit* unit)
{
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
	CMetalManager* metalMgr = circuit->GetMetalManager();
	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	if (utils::is_valid(buildPos)) {
		int index = metalMgr->FindNearestSpot(buildPos);
		if (index >= 0) {
			if (circuit->GetMap()->IsPossibleToBuildAt(buildDef->GetDef(), buildPos, facing)) {
				if ((State::ENGAGE == state) || metalMgr->IsOpenSpot(index)) {  // !isFirstTry
					state = State::ENGAGE;  // isFirstTry = false
//					metalMgr->SetOpenSpot(index, false);
					TRY_UNIT(circuit, unit,
						unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
					)
					return;
				} else {
					economyMgr->SetOpenSpot(index, true);
				}
			} else {
				metalMgr->SetOpenSpot(index, true);
				economyMgr->SetOpenSpot(index, true);
				if (!CheckLandBlock(unit)) {
					// Fallback to Guard/Assist/Patrol
					manager->FallbackTask(unit);
				}
			}
		}
	}
}

void CBMexTask::OnUnitIdle(CCircuitUnit* unit)
{
	if ((target == nullptr) && (State::ENGAGE == state)) {
		const bool isBlocked = manager->GetCircuit()->GetTerrainManager()->IsWaterSector(buildPos)
				? CheckWaterBlock(unit)
				: CheckLandBlock(unit);
		if (isBlocked) {
			return;
		}
	}

	IBuilderTask::OnUnitIdle(unit);
}

void CBMexTask::SetBuildPos(const AIFloat3& pos)
{
	FindFacing(pos);
	IBuilderTask::SetBuildPos(pos);
}

bool CBMexTask::CheckLandBlock(CCircuitUnit* unit)
{
	/*
	 * Check if unit is idle because of enemy mex ahead and build turret if so.
	 */
	if (blockCount > 2) {
		return false;
	}
	CCircuitAI* circuit = manager->GetCircuit();

	const float range = unit->GetCircuitDef()->GetLosRadius();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	const float sqPosDist = buildPos.SqDistance2D(pos);
	if (sqPosDist > SQUARE(range + 200.0f)) {
		return false;
	}
	++blockCount;

	COOAICallback* clb = circuit->GetCallback();
	Unit* enemy = nullptr;
	bool blocked = sqPosDist < SQUARE(unit->GetCircuitDef()->GetBuildDistance() + SQUARE_SIZE);
	if (blocked) {
		auto& allies = clb->GetFriendlyUnitIdsIn(buildPos, SQUARE_SIZE);
		for (int allyId : allies) {
			if (allyId != -1) {
				blocked = false;
				break;
			}
		}
	}
	auto& enemies = clb->GetEnemyUnitIdsIn(buildPos, SQUARE_SIZE);
	for (int enemyId : enemies) {
		if (enemyId != -1) {
			blocked = true;
			CEnemyInfo* ei = circuit->GetEnemyInfo(enemyId);
			if (ei != nullptr) {
				enemy = ei->GetUnit();
			}
			break;
		}
	}
	if (!blocked) {
		return false;
	}

	if (enemy != nullptr) {
		state = State::REGROUP;
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->ReclaimUnit(enemy);
		);
	} else {
		AIFloat3 dir = (buildPos - pos).Normalize2D();
		const float step = unit->GetCircuitDef()->GetBuildDistance() / 4;
		TRY_UNIT(circuit, unit,
			unit->CmdMoveTo(pos + dir * step);
			for (int i = 2; i < 4; ++i) {
				AIFloat3 newPos = pos + dir * (step * i);
				unit->CmdMoveTo(newPos, UNIT_COMMAND_OPTION_SHIFT_KEY);
			}
			unit->CmdBuild(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_SHIFT_KEY, frame + FRAMES_PER_SEC * 60);
		);
	}
	return true;
}

bool CBMexTask::CheckWaterBlock(CCircuitUnit* unit)
{
	/*
	 * Check if unit is idle because of enemy mex ahead and build turret if so.
	 */
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitDef* def = circuit->GetMilitaryManager()->GetLowSonar(unit);
	if (def == nullptr) {
		return false;
	}

	const float range = def->GetSonarRadius();
	const float testRange = range + 200.0f;  // 200 elmos
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	const float sqPosDist = buildPos.SqDistance2D(pos);
	if (sqPosDist > SQUARE(testRange)) {
		return false;
	}

	COOAICallback* clb = circuit->GetCallback();
	bool blocked = sqPosDist < SQUARE(unit->GetCircuitDef()->GetBuildDistance() + SQUARE_SIZE);
	if (blocked) {
		auto& allies = clb->GetFriendlyUnitIdsIn(buildPos, SQUARE_SIZE);
		for (int allyId : allies) {
			if (allyId != -1) {
				blocked = false;
				break;
			}
		}
	}
	auto& enemies = clb->GetEnemyUnitIdsIn(buildPos, SQUARE_SIZE);
	for (int enemyId : enemies) {
		if (enemyId != -1) {
			blocked = true;
			break;
		}
	}
	if (!blocked) {
		return false;
	}

	// TODO: send sonar scout to position
	CBuilderManager* builderMgr = circuit->GetBuilderManager();
	IBuilderTask* task = nullptr;
	const float qdist = SQUARE(200.0f);  // 200 elmos
	// TODO: Push tasks into bgi::rtree
	for (IBuilderTask* t : builderMgr->GetTasks(IBuilderTask::BuildType::DEFENCE)) {
		if (pos.SqDistance2D(t->GetTaskPos()) < qdist) {
			task = t;
			break;
		}
	}
	if (task == nullptr) {
//		AIFloat3 newPos = AIFloat3(buildPos - (buildPos - pos).Normalize2D() * (unit->GetCircuitDef()->GetBuildDistance() * 0.9f));
//		CTerrainManager::CorrectPosition(newPos);
		task = builderMgr->EnqueueTask(IBuilderTask::Priority::NOW, def, pos/*newPos*/, IBuilderTask::BuildType::DEFENCE, 1.f, 0.f, true);
	}
	manager->AssignTask(unit, task);
	return true;
}

} // namespace circuit
