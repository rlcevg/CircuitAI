/*
 * TaskModuleScript.cpp
 *
 *  Created on: Jan 2, 2021
 *      Author: rlcevg
 */

#include "script/TaskModuleScript.h"
#include "script/ScriptManager.h"
#include "module/TaskModule.h"
#include "util/Utils.h"
#include "angelscript/include/angelscript.h"

namespace circuit {

ITaskModuleScript::ITaskModuleScript(CScriptManager* scr, ITaskModule* mod)
		: IModuleScript(scr, mod)
{
}

ITaskModuleScript::~ITaskModuleScript()
{
}

void ITaskModuleScript::InitModule(asIScriptModule* mod)
{
	IModuleScript::InitModule(mod);
	umInfo.makeTask = script->GetFunc(mod, "IUnitTask@ AiMakeTask(CCircuitUnit@)");
	umInfo.taskAdded = script->GetFunc(mod, "void AiTaskAdded(IUnitTask@)");
	umInfo.taskRemoved = script->GetFunc(mod, "void AiTaskRemoved(IUnitTask@, bool)");
}

IUnitTask* ITaskModuleScript::MakeTask(CCircuitUnit* unit)
{
	if (umInfo.makeTask == nullptr) {
		return static_cast<ITaskModule*>(manager)->DefaultMakeTask(unit);
	}
	asIScriptContext* ctx = script->PrepareContext(umInfo.makeTask);
	ctx->SetArgObject(0, unit);
	IUnitTask* result = script->Exec(ctx) ? (IUnitTask*)ctx->GetReturnObject() : nullptr;
	script->ReturnContext(ctx);
	return result;
}

void ITaskModuleScript::TaskAdded(IUnitTask* task)
{
	if (umInfo.taskAdded == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(umInfo.taskAdded);
	ctx->SetArgObject(0, task);
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

void ITaskModuleScript::TaskRemoved(IUnitTask* task, bool done)
{
	if (umInfo.taskRemoved == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(umInfo.taskRemoved);
	ctx->SetArgObject(0, task);
	ctx->SetArgByte(1, done);
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

} // namespace circuit
