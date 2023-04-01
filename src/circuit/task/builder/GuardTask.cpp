/*
 * GuardTask.cpp
 *
 *  Created on: Jul 13, 2016
 *      Author: rlcevg
 */

#include "task/builder/GuardTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
#include "terrain/TerrainManager.h"  // for CorrectPosition
#include "CircuitAI.h"
#include "util/Utils.h"

#include "AISCommands.h"

namespace circuit {

CBGuardTask::CBGuardTask(ITaskManager* mgr, Priority priority, CCircuitUnit* vip, bool isInterrupt, int timeout)
		: IBuilderTask(mgr, priority, nullptr, vip->GetPos(mgr->GetCircuit()->GetLastFrame()),
					   Type::BUILDER, BuildType::GUARD, 0.f, 0.f, timeout)
		, vipId(vip->GetId())
		, isInterrupt(isInterrupt)
{
}

CBGuardTask::~CBGuardTask()
{
}

bool CBGuardTask::CanAssignTo(CCircuitUnit* unit) const
{
	return true;
}

void CBGuardTask::AssignTo(CCircuitUnit* unit)
{
	IBuilderTask::AssignTo(unit);

	if (!unit->GetCircuitDef()->GetBuildOptions().empty()) {
		static_cast<CBuilderManager*>(manager)->IncGuardCount();
		if (IsTargetBuilder()) {
			static_cast<CBuilderManager*>(manager)->DelBuildPower(unit);
		}
	}

	if (!isInterrupt) {
		lastTouched = manager->GetCircuit()->GetLastFrame();
	}
}

void CBGuardTask::RemoveAssignee(CCircuitUnit* unit)
{
	IBuilderTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}

	if (!unit->GetCircuitDef()->GetBuildOptions().empty()) {
		static_cast<CBuilderManager*>(manager)->DecGuardCount();
		if (IsTargetBuilder()) {
			static_cast<CBuilderManager*>(manager)->AddBuildPower(unit);
		}
	}
}

void CBGuardTask::Stop(bool done)
{
	const bool isVIPBuilder = IsTargetBuilder();
	for (CCircuitUnit* unit : units) {
		if (!unit->GetCircuitDef()->GetBuildOptions().empty()) {
			static_cast<CBuilderManager*>(manager)->DecGuardCount();
			if (isVIPBuilder) {
				static_cast<CBuilderManager*>(manager)->AddBuildPower(unit);
			}
		}
	}

	IBuilderTask::Stop(done);
}

bool CBGuardTask::Execute(CCircuitUnit* unit)
{
	executors.insert(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip != nullptr) {
		const int frame = circuit->GetLastFrame();
		const AIFloat3& vipPos = vip->GetPos(frame);
		const AIFloat3& unitPos = unit->GetPos(frame);
		TRY_UNIT(circuit, unit,
			unit->CmdPriority(ClampPriority());
			short options = UNIT_CMD_OPTION;
			// FIXME: it's not "Smooth area" and is broken when waterlevel is changed
//			if (unit->GetCircuitDef()->IsAbleToRestore()) {
//				unit->GetUnit()->RestoreArea(vip->GetPos(circuit->GetLastFrame()), 128.f);
//				options = UNIT_COMMAND_OPTION_SHIFT_KEY;
//			}
			if ((unit->GetCircuitDef()->GetBuildDistance() > 80.f) && (vipPos.SqDistance2D(unitPos) < SQUARE(48.f))) {
				AIFloat3 pos = vipPos + (unitPos - vipPos).Normalize2D() * 64.f;
				CTerrainManager::CorrectPosition(pos);
				unit->CmdMoveTo(pos, options | UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
				options = UNIT_COMMAND_OPTION_SHIFT_KEY;
			}
			unit->GetUnit()->Guard(vip->GetUnit(), options);
		)
	} else {
		manager->AbortTask(this);
		return false;
	}
	return true;
}

void CBGuardTask::OnUnitIdle(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->Guard(vip->GetUnit());
		)
	} else {
		manager->AbortTask(this);
	}
}

bool CBGuardTask::Reevaluate(CCircuitUnit* unit)
{
	if (!isInterrupt) {
		return true;
	}
	return IBuilderTask::Reevaluate(unit);
}

bool CBGuardTask::IsTargetBuilder() const
{
	CCircuitUnit* vip = manager->GetCircuit()->GetTeamUnit(vipId);
	return (vip != nullptr) && !vip->GetCircuitDef()->GetBuildOptions().empty();
}

} // namespace circuit
