/*
 * UnitTask.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "task/UnitTask.h"
#include "task/IdleTask.h"
#include "module/UnitModule.h"
#include "terrain/TerrainManager.h"
#include "terrain/path/PathQuery.h"
#include "unit/CircuitUnit.h"
#include "unit/action/AntiCapAction.h"
#include "CircuitAI.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

IUnitTask::IUnitTask(IUnitModule* mgr, Priority priority, Type type, int timeout)
		: manager(mgr)
		, type(type)
		, priority(priority)
		, state(State::ROAM)
		, timeout(timeout)
		, updCount(0)
		, isDead(false)
{
	lastTouched = manager->GetCircuit()->GetLastFrame();
}

IUnitTask::IUnitTask(IUnitModule* mgr, Type type)
		: manager(mgr)
		, type(type)
		, priority(Priority::LOW)
		, state(State::ROAM)
		, lastTouched(-1)
		, timeout(-1)
		, updCount(0)
		, isDead(false)
{
}

IUnitTask::~IUnitTask()
{
}

void IUnitTask::ClearRelease()
{
	pathQueries.clear();
	Release();
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

	TRY_UNIT(manager->GetCircuit(), unit,
		unit->RemoveWait();
	)

	if (!unit->GetCircuitDef()->IsMobile()) {
		unit->PushBack(new CAntiCapAction(unit));
	}
}

void IUnitTask::RemoveAssignee(CCircuitUnit* unit)
{
	pathQueries.erase(unit);
	units.erase(unit);
	unit->Clear();

	manager->GetIdleTask()->AssignTo(unit);

	if (units.empty()) {
		lastTouched = manager->GetCircuit()->GetLastFrame();
	}
}

void IUnitTask::Stop(bool done)
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
	pathQueries.clear();
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
	const int frame = circuit->GetLastFrame();
	AIFloat3 pos = utils::get_radial_pos(unit->GetPos(frame), SQUARE_SIZE * 32);
	CTerrainManager::CorrectPosition(pos);
	TRY_UNIT(circuit, unit,
		unit->CmdMoveTo(pos, UNIT_CMD_OPTION, frame + FRAMES_PER_SEC);
	)
}

void IUnitTask::OnTravelEnd(CCircuitUnit* unit)
{
}

void IUnitTask::OnRearmStart(CCircuitUnit* unit)
{
}

void IUnitTask::Dead()
{
	isDead = true;
	pathQueries.clear();  // free queries
}

bool IUnitTask::IsQueryReady(CCircuitUnit* unit) const
{
	const auto it = pathQueries.find(unit);
	std::shared_ptr<IPathQuery> query = (it == pathQueries.end()) ? nullptr : it->second;
	return (query == nullptr) || (IPathQuery::State::READY == query->GetState());
}

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, priority);		\
	utils::binary_##func(stream, state);		\
	utils::binary_##func(stream, lastTouched);	\
	utils::binary_##func(stream, timeout);		\
	utils::binary_##func(stream, updCount);		\
	utils::binary_##func(stream, isDead);

bool IUnitTask::Load(std::istream& is)
{
	SERIALIZE(is, read)
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | priority=%i | state=%i | lastTouched=%i | timeout=%i | updCount=%i | isDead=%i",
			__PRETTY_FUNCTION__, priority, state, lastTouched, timeout, updCount, isDead);
#endif
	return true;
}

void IUnitTask::Save(std::ostream& os) const
{
	SERIALIZE(os, write)
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | priority=%i | state=%i | lastTouched=%i | timeout=%i | updCount=%i | isDead=%i",
			__PRETTY_FUNCTION__, priority, state, lastTouched, timeout, updCount, isDead);
#endif
}

#ifdef DEBUG_VIS
void IUnitTask::Log()
{
	CCircuitAI* circuit = manager->GetCircuit();
	circuit->LOG("task: %lx | type: %i | state: %i", this, type, state);
}
#endif

} // namespace circuit
