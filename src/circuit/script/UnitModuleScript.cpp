/*
 * UnitModuleScript.cpp
 *
 *  Created on: Jan 2, 2021
 *      Author: rlcevg
 */

#include "script/UnitModuleScript.h"
#include "script/ScriptManager.h"
#include "module/UnitModule.h"
#include "util/Utils.h"
#include "angelscript/include/angelscript.h"

namespace circuit {

IUnitModuleScript::IUnitModuleScript(CScriptManager* scr, IUnitModule* mod)
		: IModuleScript(scr, mod)
{
}

IUnitModuleScript::~IUnitModuleScript()
{
}

void IUnitModuleScript::InitModule(asIScriptModule* mod)
{
	info.makeTask = script->GetFunc(mod, "IUnitTask@ AiMakeTask(CCircuitUnit@)");
	info.taskCreated = script->GetFunc(mod, "void AiTaskCreated(IUnitTask@)");
	info.taskClosed = script->GetFunc(mod, "void AiTaskClosed(IUnitTask@, bool)");
}

IUnitTask* IUnitModuleScript::MakeTask(CCircuitUnit* unit)
{
	if (info.makeTask == nullptr) {
		return static_cast<IUnitModule*>(manager)->DefaultMakeTask(unit);
	}
	asIScriptContext* ctx = script->PrepareContext(info.makeTask);
	ctx->SetArgObject(0, unit);
	IUnitTask* result = script->Exec(ctx) ? (IUnitTask*)ctx->GetReturnObject() : nullptr;
	script->ReturnContext(ctx);
	return result;
}

void IUnitModuleScript::TaskCreated(IUnitTask* task)
{
	if (info.taskCreated == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(info.taskCreated);
	ctx->SetArgObject(0, task);
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

void IUnitModuleScript::TaskClosed(IUnitTask* task, bool done)
{
	if (info.taskClosed == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(info.taskClosed);
	ctx->SetArgObject(0, task);
	ctx->SetArgByte(1, done);
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

} // namespace circuit
