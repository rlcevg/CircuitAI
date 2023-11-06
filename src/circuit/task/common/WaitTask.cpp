/*
 * WaitTask.cpp
 *
 *  Created on: Jul 24, 2016
 *      Author: rlcevg
 */

#include "task/common/WaitTask.h"
#include "module/UnitModule.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"

#include "Command.h"
#include "AISCommands.h"
#include "Sim/Units/CommandAI/Command.h"

namespace circuit {

IWaitTask::IWaitTask(IUnitModule* mgr, bool stop, int timeout)
		: IUnitTask(mgr, Priority::NORMAL, Type::WAIT, timeout)
		, isStop(stop)
{
}

IWaitTask::~IWaitTask()
{
}

void IWaitTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	lastTouched = manager->GetCircuit()->GetLastFrame();
}

void IWaitTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);

	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void IWaitTask::Start(CCircuitUnit* unit)
{
	if (!isStop) {
		return;
	}
	auto commands = unit->GetUnit()->GetCurrentCommands();
	if (commands.empty()) {
		return;
	}
	std::vector<float> params;
	params.reserve(commands.size());
	for (springai::Command* cmd : commands) {
		params.push_back(cmd->GetId());
		delete cmd;
	}
	TRY_UNIT(manager->GetCircuit(), unit,
		unit->CmdRemove(std::move(params), UNIT_COMMAND_OPTION_ALT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
	)
}

void IWaitTask::Update()
{
}

void IWaitTask::OnUnitIdle(CCircuitUnit* unit)
{
}

void IWaitTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	RemoveAssignee(unit);
}

} // namespace circuit
