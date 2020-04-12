/*
 * BuilderScript.cpp
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#include "script/BuilderScript.h"
#include "script/ScriptManager.h"
#include "module/BuilderManager.h"
#include "angelscript/include/angelscript.h"

namespace circuit {

using namespace springai;

CBuilderScript::CBuilderScript(CScriptManager* scr, CBuilderManager* mgr)
		: IScript(scr, mgr)
{
	asIScriptEngine* engine = script->GetEngine();
	int r = engine->RegisterObjectType("CBuilderManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CBuilderManager builderMgr", manager); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CBuilderManager", "uint GetWorkerCount() const", asMETHOD(CBuilderManager, GetWorkerCount), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CBuilderManager", "IUnitTask@+ MakeBuilderTask(CCircuitUnit@)", asMETHOD(CBuilderManager, MakeBuilderTask), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CBuilderManager", "IUnitTask@+ MakeCommTask(CCircuitUnit@)", asMETHOD(CBuilderManager, MakeCommTask), asCALL_THISCALL); ASSERT(r >= 0);
}

CBuilderScript::~CBuilderScript()
{
}

void CBuilderScript::Init()
{
	script->Load("builder", "manager/builder.as");
	asIScriptModule* mod = script->GetEngine()->GetModule("builder");
	info.makeTask = script->GetFunc(mod, "IUnitTask@ makeTask(CCircuitUnit@)");
}

IUnitTask* CBuilderScript::MakeTask(CCircuitUnit* unit)
{
	if (info.makeTask == nullptr) {
		return static_cast<CBuilderManager*>(manager)->MakeBuilderTask(unit);
	}
	asIScriptContext* ctx = script->PrepareContext(info.makeTask);
	ctx->SetArgObject(0, unit);
	IUnitTask* result = script->Exec(ctx) ? (IUnitTask*)ctx->GetReturnObject() : nullptr;
	script->ReturnContext(ctx);
	return result;
}

} // namespace circuit
