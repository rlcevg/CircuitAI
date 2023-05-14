/*
 * UnitModule.cpp
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#include "module/UnitModule.h"
#include "script/UnitModuleScript.h"
#include "task/UnitTask.h"
#include "CircuitAI.h"

namespace circuit {

IUnitModule::IUnitModule(CCircuitAI* circuit, IScript* script)
		: IModule(circuit, script)
		, IUnitManager()
		, ITaskManager()
{
	Init();
}

IUnitModule::~IUnitModule()
{
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

} // namespace circuit
