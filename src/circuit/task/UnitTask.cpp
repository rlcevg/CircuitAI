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
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

IUnitTask::IUnitTask(ITaskManager* mgr, Priority priority, Type type, int timeout)
		: manager(mgr)
		, priority(priority)
		, type(type)
		, state(State::ROAM)
		, timeout(timeout)
		, updCount(0)
		, isDead(false)
{
	lastTouched = manager->GetCircuit()->GetLastFrame();
}

IUnitTask::~IUnitTask()
{
}

bool IUnitTask::CanAssignTo(CCircuitUnit* unit) const
{
	return true;
}

void IUnitTask::AssignTo(CCircuitUnit* unit)
{
	lastTouched = -1;

	manager->GetIdleTask()->RemoveAssignee(unit);
	unit->SetTask(this);
	units.insert(unit);
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
		unit->Clear();
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
	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	const AIFloat3& pos = utils::get_radial_pos(unit->GetPos(frame), SQUARE_SIZE * 32);
	TRY_UNIT(circuit, unit,
		unit->GetUnit()->MoveTo(pos, 0, frame + FRAMES_PER_SEC);
	)
}

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, priority);		\
	utils::binary_##func(stream, type);			\
	utils::binary_##func(stream, state);		\
	utils::binary_##func(stream, lastTouched);	\
	utils::binary_##func(stream, timeout);		\
	utils::binary_##func(stream, updCount);		\
	utils::binary_##func(stream, isDead);

void IUnitTask::Load(std::istream& is)
{
	SERIALIZE(is, read)
}

void IUnitTask::Save(std::ostream& os) const
{
	SERIALIZE(os, write)
}

} // namespace circuit
