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
	if (unit->GetCircuitDef()->IsAttacker()) {
		return true;
	}
	// TODO: Naked expansion on big maps
	CCircuitAI* circuit = manager->GetCircuit();
	CMilitaryManager* militaryMgr = circuit->GetMilitaryManager();
	if ((militaryMgr->GetDefendTaskNum() == 0) || (circuit->GetLastFrame() > militaryMgr->GetDefendFrame())) {
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
			unit->GetUnit()->Repair(target->GetUnit(), UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
		)
		return;
	}
	CMetalManager* metalMgr = circuit->GetMetalManager();
	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	UnitDef* buildUDef = buildDef->GetDef();
	if (utils::is_valid(buildPos)) {
		int index = metalMgr->FindNearestSpot(buildPos);
		if (index >= 0) {
			if (circuit->GetMap()->IsPossibleToBuildAt(buildUDef, buildPos, facing)) {
				if ((State::ENGAGE == state) || metalMgr->IsOpenSpot(index)) {  // !isFirstTry
					state = State::ENGAGE;  // isFirstTry = false
//					metalMgr->SetOpenSpot(index, false);
					TRY_UNIT(circuit, unit,
						unit->GetUnit()->Build(buildUDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
					)
					return;
				} else {
					economyMgr->SetOpenSpot(index, true);
				}
			} else {
				metalMgr->SetOpenSpot(index, true);
				economyMgr->SetOpenSpot(index, true);
			}
		}
	}

	// NOTE: Unsafe fallback expansion (mex can be behind enemy lines)
	const CMetalData::Metals& spots = metalMgr->GetSpots();
	CMap* map = circuit->GetMap();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CCircuitDef* mexDef = buildDef;
	circuit->GetThreatMap()->SetThreatType(unit);
	CMetalData::PointPredicate predicate = [&spots, economyMgr, map, mexDef, terrainMgr, unit](const int index) {
		return (economyMgr->IsAllyOpenSpot(index)
				&& terrainMgr->CanBeBuiltAtSafe(mexDef, spots[index].position)  // hostile environment
				&& terrainMgr->CanReachAtSafe(unit, spots[index].position, unit->GetCircuitDef()->GetBuildDistance())
				&& map->IsPossibleToBuildAt(mexDef->GetDef(), spots[index].position, UNIT_COMMAND_BUILD_NO_FACING));
	};
	int index = metalMgr->FindNearestSpot(position, predicate);

	if (index >= 0) {
		SetBuildPos(spots[index].position);
		economyMgr->SetOpenSpot(index, false);
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Build(buildUDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
		)
	} else {
//		buildPos = -RgtVector;
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

void CBMexTask::OnUnitIdle(CCircuitUnit* unit)
{
	/*
	 * Check if unit is idle because of enemy mex ahead and build turret if so.
	 */
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitDef* def = circuit->GetMilitaryManager()->GetDefaultPorc();
	if ((def == nullptr) || !def->IsAvailable(circuit->GetLastFrame())) {
		IBuilderTask::OnUnitIdle(unit);
		return;
	}

	const float range = def->GetMaxRange();
	const float testRange = range + 200.0f;  // 200 elmos
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	if (buildPos.SqDistance2D(pos) < SQUARE(testRange)) {
		COOAICallback* clb = circuit->GetCallback();
		// TODO: Use internal CCircuitAI::GetEnemyUnits?
		auto enemies = clb->GetEnemyUnitIdsIn(buildPos, SQUARE_SIZE);
		bool blocked = false;
		for (int enemyId : enemies) {
			if (enemyId == -1) {
				continue;
			}
			CCircuitDef::Id enemyDefId = clb->Unit_GetDefId(enemyId);
			if ((enemyDefId != -1) && circuit->GetCircuitDef(enemyDefId)->IsMex()) {
				blocked = true;
				break;
			}
		}
		if (blocked) {
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
				AIFloat3 newPos = buildPos - (buildPos - pos).Normalize2D() * range * 0.9f;
				CTerrainManager::CorrectPosition(newPos);
				task = builderMgr->EnqueueTask(IBuilderTask::Priority::HIGH, def, newPos, IBuilderTask::BuildType::DEFENCE);
			}
			// TODO: Before BuildTask assign MoveTask(task->GetTaskPos())
			manager->AssignTask(unit, task);
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

} // namespace circuit
