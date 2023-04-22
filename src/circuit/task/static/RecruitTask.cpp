/*
 * RecruitTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "task/static/RecruitTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "module/FactoryManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
#include "unit/action/DGunAction.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "Command.h"
#include "AISCommands.h"

namespace circuit {

using namespace springai;

CRecruitTask::CRecruitTask(ITaskManager* mgr, Priority priority,
						   CCircuitDef* buildDef, const AIFloat3& position,
						   RecruitType type, float radius)
		: IBuilderTask(mgr, priority, buildDef, position, Type::FACTORY, BuildType::RECRUIT, {0.f, 0.f}, 0.f, -1)
		, recruitType(type)
		, sqradius(radius * radius)
{
}

CRecruitTask::~CRecruitTask()
{
}

bool CRecruitTask::CanAssignTo(CCircuitUnit* unit) const
{
	return (target == nullptr) && unit->GetCircuitDef()->CanBuild(buildDef) &&
		   (position.SqDistance2D(unit->GetPos(manager->GetCircuit()->GetLastFrame())) <= sqradius);
}

void CRecruitTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	ShowAssignee(unit);
	if (!utils::is_valid(position)) {
		position = unit->GetPos(circuit->GetLastFrame());
	}

	if (unit->HasDGun()) {
		unit->PushDGunAct(new CDGunAction(unit, unit->GetDGunRange()));
	}
}

void CRecruitTask::Start(CCircuitUnit* unit)
{
	Execute(unit);
}

void CRecruitTask::Update()
{
	if (units.empty()) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	CEconomyManager* economyMgr = circuit->GetEconomyManager();

	const bool isEnergyEmpty = economyMgr->IsEnergyEmpty();
	for (CCircuitUnit* unit : units) {
		TRY_UNIT(circuit, unit,
			unit->CmdWait(isEnergyEmpty);
		)
	}
	if (isEnergyEmpty) {
		return;
	}

	bool hasMetal = economyMgr->GetAvgMetalIncome() * 2.0f > economyMgr->GetMetalPull();
	if (State::DISENGAGE == state) {
		if (hasMetal) {
			state = State::ROAM;  // Not wait
			for (CCircuitUnit* unit : units) {
				TRY_UNIT(circuit, unit,
					unit->CmdPriority(ClampPriority());
					unit->CmdBARPriority(ClampPriority());
				)
			}
		}
	} else {
		if (!hasMetal) {
			state = State::DISENGAGE;  // Wait
			for (CCircuitUnit* unit : units) {
				TRY_UNIT(circuit, unit,
					unit->CmdPriority(0);
					unit->CmdBARPriority(0);
				)
			}
		}
	}
}

void CRecruitTask::Finish()
{
	Cancel();

	CCircuitAI* circuit = manager->GetCircuit();
	const int buildDelay = circuit->GetEconomyManager()->GetBuildDelay();
	if (buildDelay > 0) {
		// NOTE: force-stop as there could be bug present that queues more than 1 unit
		IUnitTask* task = circuit->GetFactoryManager()->EnqueueWait(true, buildDelay);
		decltype(units) tmpUnits = units;
		for (CCircuitUnit* unit : tmpUnits) {
			manager->AssignTask(unit, task);
		}
	}
}

void CRecruitTask::Cancel()
{
	if (target != nullptr) {
		SetTarget(nullptr);
	}

	CCircuitAI* circuit = manager->GetCircuit();
	for (CCircuitUnit* unit : units) {
		// Clear build-queue
		auto commands = unit->GetUnit()->GetCurrentCommands();
		std::vector<float> params;
		params.reserve(commands.size());
		for (springai::Command* cmd : commands) {
			int cmdId = cmd->GetId();
			if (cmdId < 0) {
				params.push_back(cmdId);
			}
			delete cmd;
		}
		TRY_UNIT(circuit, unit,
			unit->CmdRemove(std::move(params), UNIT_COMMAND_OPTION_ALT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
		)
	}
}

bool CRecruitTask::Execute(CCircuitUnit* unit)
{
	executors.insert(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	TRY_UNIT(circuit, unit,
		unit->CmdPriority(ClampPriority());
		unit->CmdBARPriority(ClampPriority());
	)
	const int frame = circuit->GetLastFrame();

	if (unit->GetCircuitDef()->IsHub()) {
		AIFloat3 pos = unit->GetPos(frame);
		const float size = DEFAULT_SLACK / 2;
		switch (unit->GetUnit()->GetBuildingFacing()) {
			default:
			case UNIT_FACING_SOUTH: {  // z++
				pos.z += size;
			} break;
			case UNIT_FACING_EAST: {  // x++
				pos.x += size;
			} break;
			case UNIT_FACING_NORTH: {  // z--
				pos.z -= size;
			} break;
			case UNIT_FACING_WEST: {  // x--
				pos.x -= size;
			} break;
		}
		buildPos = circuit->GetTerrainManager()->FindBuildSite(buildDef, pos, unit->GetCircuitDef()->GetBuildDistance(), UNIT_NO_FACING, true);
//		buildPos = circuit->GetTerrainManager()->FindSpringBuildSite(buildDef, pos, unit->GetCircuitDef()->GetBuildDistance(), UNIT_NO_FACING);
	} else {
		// factory
		buildPos = unit->GetPos(frame);
	}

	if (utils::is_valid(buildPos)) {
		TRY_UNIT(circuit, unit,
			unit->CmdBuild(buildDef, buildPos, UNIT_NO_FACING, 0, frame + FRAMES_PER_SEC * 10);
		)
	} else {
		manager->AbortTask(this);
		return false;
	}
	return true;
}

void CRecruitTask::OnUnitIdle(CCircuitUnit* unit)
{
	Start(unit);
}

void CRecruitTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	// TODO: React: analyze, abort, create appropriate task
}

void CRecruitTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	RemoveAssignee(unit);
}

void CRecruitTask::SetTarget(CCircuitUnit* unit)
{
	for (CCircuitUnit* worker : units) {
		if (!worker->GetCircuitDef()->IsHub()) {  // do not replace or remove blocker
			buildPos = -RgtVector;
			break;
		}
	}
	IBuilderTask::SetTarget(unit);
}

} // namespace circuit
