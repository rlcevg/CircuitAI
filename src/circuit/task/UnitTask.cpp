/*
 * UnitTask.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "task/UnitTask.h"
#include "task/IdleTask.h"
#include "task/TaskManager.h"
#include "unit/CircuitUnit.h"
#include "unit/action/IdleAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

IUnitTask::IUnitTask(ITaskManager* mgr, Priority priority, Type type, int timeout)
		: manager(mgr)
		, priority(priority)
		, type(type)
		, timeout(timeout)
		, updCount(0)
{
	lastTouched = manager->GetCircuit()->GetLastFrame();
}

IUnitTask::~IUnitTask()
{
}

bool IUnitTask::CanAssignTo(CCircuitUnit* unit)
{
	return true;
}

void IUnitTask::AssignTo(CCircuitUnit* unit)
{
	lastTouched = -1;

	manager->GetIdleTask()->RemoveAssignee(unit);
	unit->SetTask(this);
	units.insert(unit);

	unit->PushBack(new CIdleAction(unit));
}

void IUnitTask::RemoveAssignee(CCircuitUnit* unit)
{
	units.erase(unit);
	unit->Clear();

	manager->GetIdleTask()->AssignTo(unit);

	if (units.empty()) {
		lastTouched = manager->GetCircuit()->GetLastFrame();
	}
}

void IUnitTask::Close(bool done)
{
	if (done) {
		Finish();
	} else {
		Cancel();
	}

	CIdleTask* idleTask = manager->GetIdleTask();
	for (CCircuitUnit* unit : units) {
		idleTask->AssignTo(unit);
	}
	units.clear();
}

void IUnitTask::Finish()
{
}

void IUnitTask::Cancel()
{
}

void IUnitTask::OnUnitMoveFailed(CCircuitUnit* unit)
{
	int frame = manager->GetCircuit()->GetLastFrame();
	const AIFloat3& pos = utils::get_radial_pos(unit->GetPos(frame), SQUARE_SIZE * 32);
	unit->GetUnit()->MoveTo(pos, 0, frame + FRAMES_PER_SEC);
}

} // namespace circuit
