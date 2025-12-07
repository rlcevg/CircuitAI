/*
 * TaskModule.cpp
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#include "module/TaskModule.h"
#include "script/TaskModuleScript.h"
#include "task/NilTask.h"
#include "task/IdleTask.h"
#include "task/PlayerTask.h"
#include "task/RetreatTask.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Profiler.h"

namespace circuit {

ITaskModule::ITaskModule(CCircuitAI* circuit, IScript* script)
		: IModule(circuit, script)
		, nilTask(nullptr)
		, idleTask(nullptr)
		, playerTask(nullptr)
		, updateIterator(0)
		, metalPull(0.f)
{
	Init();
}

ITaskModule::~ITaskModule()
{
	delete nilTask;
	delete idleTask;
	delete playerTask;

	for (IUnitTask* task : updateTasks) {
		task->ClearRelease();
	}
}

void ITaskModule::Init()
{
	nilTask = new CNilTask(this);
	idleTask = new CIdleTask(this);
	playerTask = new CPlayerTask(this);
}

void ITaskModule::Release()
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

void ITaskModule::AssignTask(CCircuitUnit* unit, IUnitTask* task)
{
	unit->GetTask()->RemoveAssignee(unit);
	task->AssignTo(unit);
	task->Start(unit);
}

void ITaskModule::AssignTask(CCircuitUnit* unit)
{
	IUnitTask* task = MakeTask(unit);
	if (task != nullptr) {
		task->AssignTo(unit);
	}
}

void ITaskModule::DequeueTask(IUnitTask* task, bool done)
{
	task->Dead();
	TaskRemoved(task, done);
	task->Stop(done);
}

IUnitTask* ITaskModule::MakeTask(CCircuitUnit* unit)
{
	return static_cast<ITaskModuleScript*>(script)->MakeTask(unit);  // DefaultMakeTask
}

void ITaskModule::TaskAdded(IUnitTask* task)
{
	static_cast<ITaskModuleScript*>(script)->TaskAdded(task);
}

void ITaskModule::TaskRemoved(IUnitTask* task, bool done)
{
	static_cast<ITaskModuleScript*>(script)->TaskRemoved(task, done);
}

void ITaskModule::AssignPlayerTask(CCircuitUnit* unit)
{
	AssignTask(unit, playerTask);
}

void ITaskModule::Resurrected(CCircuitUnit* unit)
{
	CRetreatTask* task = EnqueueRetreat();
	if (task != nullptr) {
		AssignTask(unit, task);
	}
}

void ITaskModule::UpdateIdle()
{
	ZoneScoped;

	idleTask->Update();
}

void ITaskModule::Update()
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
