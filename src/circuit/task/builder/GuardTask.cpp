/*
 * GuardTask.cpp
 *
 *  Created on: Jul 13, 2016
 *      Author: rlcevg
 */

#include "task/builder/GuardTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
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
		static_cast<CBuilderManager*>(manager)->DelBuildPower(unit);
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
		static_cast<CBuilderManager*>(manager)->AddBuildPower(unit);
	}
}

void CBGuardTask::Stop(bool done)
{
	for (CCircuitUnit* unit : units) {
		if (!unit->GetCircuitDef()->GetBuildOptions().empty()) {
			static_cast<CBuilderManager*>(manager)->AddBuildPower(unit);
		}
	}

	IBuilderTask::Stop(done);
}

void CBGuardTask::Execute(CCircuitUnit* unit)
{
	executors.insert(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->CmdPriority(ClampPriority());
			const bool isRestore = unit->GetCircuitDef()->IsAbleToRestore();
			if (isRestore) {
				unit->GetUnit()->RestoreArea(vip->GetPos(circuit->GetLastFrame()), 128.f);
			}
			unit->GetUnit()->Guard(vip->GetUnit(), isRestore ? UNIT_COMMAND_OPTION_SHIFT_KEY : 0);
		)
	} else {
		manager->AbortTask(this);
	}
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

} // namespace circuit
