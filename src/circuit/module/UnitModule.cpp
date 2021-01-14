/*
 * UnitModule.cpp
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#include "module/UnitModule.h"
#include "script/UnitModuleScript.h"
#include "task/UnitTask.h"

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

CCircuitAI* IUnitModule::GetCircuit()
{
	return circuit;
}

void IUnitModule::DequeueTask(IUnitTask* task, bool done)
{
	task->Dead();
	TaskClosed(task, done);
	task->Stop(done);
}

IUnitTask* IUnitModule::MakeTask(CCircuitUnit* unit)
{
	return static_cast<IUnitModuleScript*>(script)->MakeTask(unit);  // DefaultMakeTask
}

void IUnitModule::TaskCreated(IUnitTask* task)
{
	static_cast<IUnitModuleScript*>(script)->TaskCreated(task);
}

void IUnitModule::TaskClosed(IUnitTask* task, bool done)
{
	static_cast<IUnitModuleScript*>(script)->TaskClosed(task, done);
}

} // namespace circuit
