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
#include "util/utils.h"

#include "Command.h"
#include "AISCommands.h"
#include "Sim/Units/CommandAI/Command.h"

namespace circuit {

using namespace springai;

CRecruitTask::CRecruitTask(ITaskManager* mgr, Priority priority,
						   CCircuitDef* buildDef, const AIFloat3& position,
						   RecruitType type, float radius)
		: IBuilderTask(mgr, priority, buildDef, position, Type::FACTORY, BuildType::RECRUIT, .0f, .0f, -1)
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
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	bool hasMetal = economyManager->GetAvgMetalIncome() * 2.0f > economyManager->GetMetalPull();
	if (State::DISENGAGE == state) {
		if (hasMetal) {
			state = State::ROAM;  // Not wait
			for (CCircuitUnit* unit : units) {
				TRY_UNIT(circuit, unit,
					unit->GetUnit()->ExecuteCustomCommand(CMD_PRIORITY, {ClampPriority()});
				)
			}
		}
	} else {
		if (!hasMetal) {
			state = State::DISENGAGE;  // Wait
			for (CCircuitUnit* unit : units) {
				TRY_UNIT(circuit, unit,
					unit->GetUnit()->ExecuteCustomCommand(CMD_PRIORITY, {0});
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
		IUnitTask* task = circuit->GetFactoryManager()->EnqueueWait(false, buildDelay);
		decltype(units) tmpUnits = units;
		for (CCircuitUnit* unit : tmpUnits) {
			manager->AssignTask(unit, task);
		}
	}
}

void CRecruitTask::Cancel()
{
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
			unit->GetUnit()->ExecuteCustomCommand(CMD_REMOVE, params, UNIT_COMMAND_OPTION_ALT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
		)
	}
}

void CRecruitTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	TRY_UNIT(circuit, unit,
		unit->GetUnit()->ExecuteCustomCommand(CMD_PRIORITY, {ClampPriority()});
	)
	const int frame = circuit->GetLastFrame();

	const float buildDistance = unit->GetCircuitDef()->GetBuildDistance();
	if (buildDistance > 200.0f) {
		// striderhub
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
		buildPos = circuit->GetTerrainManager()->FindBuildSite(buildDef, pos, buildDistance, UNIT_COMMAND_BUILD_NO_FACING);
	} else {
		// factory
		buildPos = unit->GetPos(frame);
	}

	if (utils::is_valid(buildPos)) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Build(buildDef->GetDef(), buildPos, UNIT_COMMAND_BUILD_NO_FACING, 0, frame + FRAMES_PER_SEC * 10);
		)
	} else {
		manager->AbortTask(this);
	}
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

} // namespace circuit
