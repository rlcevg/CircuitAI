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

namespace circuit {

CBGuardTask::CBGuardTask(ITaskManager* mgr, Priority priority, CCircuitUnit* vip, int timeout)
		: IBuilderTask(mgr, priority, nullptr, vip->GetPos(mgr->GetCircuit()->GetLastFrame()),
					   Type::BUILDER, BuildType::GUARD, 0.f, 0.f, timeout)
		, vipId(vip->GetId())
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

	static_cast<CBuilderManager*>(manager)->DelBuildPower(unit);
}

void CBGuardTask::RemoveAssignee(CCircuitUnit* unit)
{
	IBuilderTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}

	static_cast<CBuilderManager*>(manager)->AddBuildPower(unit);
}

void CBGuardTask::Stop(bool done)
{
	for (CCircuitUnit* unit : units) {
		static_cast<CBuilderManager*>(manager)->AddBuildPower(unit);
	}

	IBuilderTask::Stop(done);
}

void CBGuardTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* vip = circuit->GetTeamUnit(vipId);
	if (vip != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->CmdPriority(ClampPriority());
			unit->GetUnit()->Guard(vip->GetUnit());
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

} // namespace circuit
