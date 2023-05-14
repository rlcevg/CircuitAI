/*
 * UnitModuleScript.cpp
 *
 *  Created on: Jan 2, 2021
 *      Author: rlcevg
 */

#include "script/UnitModuleScript.h"
#include "script/ScriptManager.h"
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
	IModuleScript::InitModule(mod);
	umInfo.makeTask = script->GetFunc(mod, "IUnitTask@ AiMakeTask(CCircuitUnit@)");
	umInfo.taskAdded = script->GetFunc(mod, "void AiTaskAdded(IUnitTask@)");
	umInfo.taskRemoved = script->GetFunc(mod, "void AiTaskRemoved(IUnitTask@, bool)");
	umInfo.unitAdded = script->GetFunc(mod, "void AiUnitAdded(CCircuitUnit@, Unit::UseAs usage)");
	umInfo.unitRemoved = script->GetFunc(mod, "void AiUnitRemoved(CCircuitUnit@, Unit::UseAs usage)");
}

IUnitTask* IUnitModuleScript::MakeTask(CCircuitUnit* unit)
{
	if (umInfo.makeTask == nullptr) {
		return static_cast<IUnitModule*>(manager)->DefaultMakeTask(unit);
	}
	asIScriptContext* ctx = script->PrepareContext(umInfo.makeTask);
	ctx->SetArgObject(0, unit);
	IUnitTask* result = script->Exec(ctx) ? (IUnitTask*)ctx->GetReturnObject() : nullptr;
	script->ReturnContext(ctx);
	return result;
}

void IUnitModuleScript::TaskAdded(IUnitTask* task)
{
	if (umInfo.taskAdded == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(umInfo.taskAdded);
	ctx->SetArgObject(0, task);
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

void IUnitModuleScript::TaskRemoved(IUnitTask* task, bool done)
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

void IUnitModuleScript::UnitAdded(CCircuitUnit* unit, IUnitModule::UseAs usage)
{
	if (umInfo.unitAdded == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(umInfo.unitAdded);
	ctx->SetArgObject(0, unit);
	ctx->SetArgDWord(1, static_cast<std::underlying_type<decltype(usage)>::type>(usage));
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

void IUnitModuleScript::UnitRemoved(CCircuitUnit* unit, IUnitModule::UseAs usage)
{
	if (umInfo.unitRemoved == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(umInfo.unitRemoved);
	ctx->SetArgObject(0, unit);
	ctx->SetArgDWord(1, static_cast<std::underlying_type<decltype(usage)>::type>(usage));
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

} // namespace circuit
