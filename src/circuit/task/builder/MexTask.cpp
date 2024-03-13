/*
 * MexTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/MexTask.h"
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

CBMexTask::CBMexTask(IUnitModule* mgr, Priority priority,
					 CCircuitDef* buildDef, int spotId, const AIFloat3& position,
					 SResource cost, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::MEX, cost, 0.f, timeout)
		, spotId(spotId)
		, blockCount(0)
{
	SetBuildPos(position);
	manager->GetCircuit()->GetEconomyManager()->SetOpenMexSpot(spotId, false);
}

CBMexTask::CBMexTask(IUnitModule* mgr)
		: IBuilderTask(mgr, Type::BUILDER, BuildType::MEX)
		, spotId(-1)
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
	if (circuit->GetEconomyManager()->IsEnergyStalling() && (circuit->GetBuilderManager()->GetWorkerCount() <= 2)) {
		return false;
	}
	if (unit->GetCircuitDef()->IsAttacker()) {
		return true;
	}
	CMilitaryManager* militaryMgr = circuit->GetMilitaryManager();
	if ((militaryMgr->GetGuardTaskNum() == 0) || (circuit->GetLastFrame() > militaryMgr->GetGuardFrame())) {
		return true;
	}
	int cluster = circuit->GetMetalManager()->FindNearestCluster(buildPos);
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
		if (spotId >= 0) {  // for broken Load
			circuit->GetMetalManager()->SetOpenSpot(spotId, true);
			circuit->GetEconomyManager()->SetOpenMexSpot(spotId, true);
		}
		IBuilderTask::SetBuildPos(-RgtVector);
	}
}

bool CBMexTask::Execute(CCircuitUnit* unit)
{
	executors.insert(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	TRY_UNIT(circuit, unit,
		unit->CmdPriority(ClampPriority());
	)

	const int frame = circuit->GetLastFrame();
	if (target != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->CmdRepair(target, UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
		)
		return true;
	}
	CMetalManager* metalMgr = circuit->GetMetalManager();
	CEconomyManager* economyMgr = circuit->GetEconomyManager();
	if (circuit->GetMap()->IsPossibleToBuildAt(buildDef->GetDef(), buildPos, facing)) {
		if ((State::ENGAGE == state) || metalMgr->IsOpenSpot(spotId)) {  // !isFirstTry
			state = State::ENGAGE;  // isFirstTry = false
//			metalMgr->SetOpenSpot(index, false);
			TRY_UNIT(circuit, unit,
				unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
			)
			return true;
		} else {
			economyMgr->SetOpenMexSpot(spotId, true);
		}
	} else {
		metalMgr->SetOpenSpot(spotId, true);
		economyMgr->SetOpenMexSpot(spotId, true);
		if (!CheckLandBlock(unit)) {
			// Fallback to Guard/Assist/Patrol
			manager->FallbackTask(unit);
			return false;
		}
	}
	return true;
}

bool CBMexTask::Reevaluate(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (circuit->IsAllyAware()) {
		auto closeTask = [this, circuit]() {
			circuit->GetEconomyManager()->SetOpenMexSpot(spotId, true);
			spotId = 0;  // prevent spot opening on Cancel
			manager->AbortTask(this);
			return false;
		};
		if (circuit->GetTerrainManager()->IsZoneAlly(buildPos)) {
			return closeTask();
		}
		auto& ids = circuit->GetCallback()->GetFriendlyUnitIdsIn(buildPos, buildDef->GetExtrRangeM(), false);
		for (ICoreUnit::Id id : ids) {
			CAllyUnit* au = circuit->GetFriendlyUnit(id);
			if ((au != nullptr) && au->GetCircuitDef()->IsMex()
				&& (au->GetUnit()->GetTeam() != circuit->GetTeamId()))
			{
				return closeTask();
			}
		}
	}
	return IBuilderTask::Reevaluate(unit);
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
	bool isNeutral = false;
	auto& neutrals = clb->GetNeutralUnitsIn(buildPos, SQUARE_SIZE * 8);
	for (Unit* n : neutrals) {
		CCircuitDef* ndef = circuit->GetCircuitDefSafe(clb->Unit_GetDefId(n->GetUnitId()));
		if ((ndef == nullptr) || ndef->IsReclaimable()) {
			isNeutral = true;
			state = State::REGROUP;
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->ReclaimUnit(n);
			);
			break;
		}
	}
	utils::free(neutrals);
	if (isNeutral) {
		return true;
	}

	CEnemyInfo* enemy = nullptr;
	auto& enemies = clb->GetEnemyUnitIdsIn(buildPos, SQUARE_SIZE * 8);
	for (ICoreUnit::Id enemyId : enemies) {
		CEnemyInfo* e = circuit->GetEnemyInfo(enemyId);
		if (e == nullptr) {
			continue;
		}
		CCircuitDef* edef = e->GetCircuitDef();
		if ((edef == nullptr) || edef->IsReclaimable()) {
			enemy = e;
			break;
		}
	}

	if (enemy != nullptr) {
		state = State::REGROUP;
		TRY_UNIT(circuit, unit,
			unit->CmdReclaimEnemy(enemy);
		);
		return true;
	}

	bool blocked = sqPosDist < SQUARE(unit->GetCircuitDef()->GetBuildDistance() + SQUARE_SIZE);
	if (blocked && clb->IsFriendlyUnitsIn(buildPos, SQUARE_SIZE * 8)) {
		blocked = false;
	}
	if (blocked) {
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
		return true;
	}

	return false;
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
	if (blocked && clb->IsFriendlyUnitsIn(buildPos, SQUARE_SIZE)) {
		blocked = false;
	}
	if (!blocked && clb->IsEnemyUnitsIn(buildPos, SQUARE_SIZE)) {
		blocked = true;
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
		task = builderMgr->Enqueue(TaskB::Common(IBuilderTask::BuildType::DEFENCE,
								   IBuilderTask::Priority::NOW, def, pos/*newPos*/, 0.f, true));
	}
	manager->AssignTask(unit, task);
	return true;
}

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, spotId);

bool CBMexTask::Load(std::istream& is)
{
	IBuilderTask::Load(is);
	SERIALIZE(is, read)

	CCircuitAI* circuit = manager->GetCircuit();
	if (!circuit->GetMetalManager()->IsSpotValid(spotId, GetPosition())) {
		spotId = -1;
#ifdef DEBUG_SAVELOAD
		manager->GetCircuit()->LOG("%s | spotId=%i", __PRETTY_FUNCTION__, spotId);
#endif
		return false;
	}
	circuit->GetEconomyManager()->SetOpenMexSpot(spotId, false);
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | spotId=%i", __PRETTY_FUNCTION__, spotId);
#endif
	return true;
}

void CBMexTask::Save(std::ostream& os) const
{
	IBuilderTask::Save(os);
	SERIALIZE(os, write)
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | spotId=%i", __PRETTY_FUNCTION__, spotId);
#endif
}

} // namespace circuit
