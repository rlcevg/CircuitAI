/*
 * NilTask.cpp
 *
 *  Created on: May 29, 2015
 *      Author: rlcevg
 */

#include "task/NilTask.h"
#include "unit/CircuitUnit.h"
#include "unit/action/AntiCapAction.h"
#include "util/Utils.h"

namespace circuit {

CNilTask::CNilTask(ITaskManager* mgr)
		: IUnitTask(mgr, IUnitTask::Priority::LOW, Type::NIL, -1)
{
}

CNilTask::~CNilTask()
{
}

void CNilTask::AssignTo(CCircuitUnit* unit)
{
	unit->SetTask(this);

	if (!unit->GetCircuitDef()->IsMobile()) {
		unit->PushBack(new CAntiCapAction(unit));
	}
}

void CNilTask::RemoveAssignee(CCircuitUnit* unit)
{
	unit->Clear();
}

void CNilTask::Start(CCircuitUnit* unit)
{
}

void CNilTask::Update()
{
}

void CNilTask::Stop(bool done)
{
}

void CNilTask::OnUnitIdle(CCircuitUnit* unit)
{
}

void CNilTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
}

void CNilTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
}

} // namespace circuit
