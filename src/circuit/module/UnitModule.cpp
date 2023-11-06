/*
 * UnitModule.cpp
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#include "module/UnitModule.h"
#include "script/UnitModuleScript.h"
#include "task/NilTask.h"
#include "task/IdleTask.h"
#include "task/PlayerTask.h"
#include "task/RetreatTask.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Profiler.h"

namespace circuit {

IUnitModule::IUnitModule(CCircuitAI* circuit, IScript* script)
		: IModule(circuit, script)
		, nilTask(nullptr)
		, idleTask(nullptr)
		, playerTask(nullptr)
		, updateIterator(0)
		, metalPull(0.f)
{
	Init();
}

IUnitModule::~IUnitModule()
{
	delete nilTask;
	delete idleTask;
	delete playerTask;

	for (IUnitTask* task : updateTasks) {
		task->ClearRelease();
	}
}

void IUnitModule::Init()
{
	nilTask = new CNilTask(this);
	idleTask = new CIdleTask(this);
	playerTask = new CPlayerTask(this);
}

void IUnitModule::Release()
{
	// NOTE: Release expected to be called on CCircuit::Release.
	//       It doesn't stop scheduled GameTasks for that reason.
	for (IUnitTask* task : updateTasks) {
		AbortTask(task);
		// NOTE: Do not delete task as other AbortTask may ask for it
	}
	for (IUnitTask* task : updateTasks) {
		task->ClearRelease();
	}
	updateTasks.clear();
}

void IUnitModule::AssignTask(CCircuitUnit* unit, IUnitTask* task)
{
	unit->GetTask()->RemoveAssignee(unit);
	task->AssignTo(unit);
	task->Start(unit);
}

void IUnitModule::AssignTask(CCircuitUnit* unit)
{
	IUnitTask* task = MakeTask(unit);
	if (task != nullptr) {
		task->AssignTo(unit);
	}
}

void IUnitModule::DequeueTask(IUnitTask* task, bool done)
{
	task->Dead();
	TaskRemoved(task, done);
	task->Stop(done);
}

IUnitTask* IUnitModule::MakeTask(CCircuitUnit* unit)
{
	return static_cast<IUnitModuleScript*>(script)->MakeTask(unit);  // DefaultMakeTask
}

void IUnitModule::TaskAdded(IUnitTask* task)
{
	static_cast<IUnitModuleScript*>(script)->TaskAdded(task);
}

void IUnitModule::TaskRemoved(IUnitTask* task, bool done)
{
	static_cast<IUnitModuleScript*>(script)->TaskRemoved(task, done);
}

void IUnitModule::UnitAdded(CCircuitUnit* unit, UseAs usage)
{
	if (circuit->IsLoadSave()) {
		return;
	}
	static_cast<IUnitModuleScript*>(script)->UnitAdded(unit, usage);
}

void IUnitModule::UnitRemoved(CCircuitUnit* unit, UseAs usage)
{
	static_cast<IUnitModuleScript*>(script)->UnitRemoved(unit, usage);
}

void IUnitModule::AssignPlayerTask(CCircuitUnit* unit)
{
	AssignTask(unit, playerTask);
}

void IUnitModule::Resurrected(CCircuitUnit* unit)
{
	CRetreatTask* task = EnqueueRetreat();
	if (task != nullptr) {
		AssignTask(unit, task);
	}
}

void IUnitModule::UpdateIdle()
{
	ZoneScoped;

	idleTask->Update();
}

void IUnitModule::Update()
{
	ZoneScoped;

	if (updateIterator >= updateTasks.size()) {
		updateIterator = 0;
	}

	int lastFrame = GetCircuit()->GetLastFrame();
	// stagger the Update's
	unsigned int n = (updateTasks.size() / TEAM_SLOWUPDATE_RATE) + 1;

	while ((updateIterator < updateTasks.size()) && (n != 0)) {
		IUnitTask* task = updateTasks[updateIterator];
		if (task->IsDead()) {
			updateTasks[updateIterator] = updateTasks.back();
			updateTasks.pop_back();
			task->ClearRelease();  // delete task;
		} else {
			// NOTE: IFighterTask.timeout = 0
			int frame = task->GetLastTouched();
			int timeout = task->GetTimeout();
			if ((frame != -1) && (timeout > 0) && (lastFrame - frame >= timeout)) {
				AbortTask(task);
			} else {
				task->Update();
			}
			++updateIterator;
			n--;
		}
	}
}

} // namespace circuit
