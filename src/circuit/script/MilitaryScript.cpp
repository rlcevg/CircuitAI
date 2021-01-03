/*
 * MilitaryScript.cpp
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#include "script/MilitaryScript.h"
#include "script/ScriptManager.h"
#include "module/MilitaryManager.h"
#include "util/Utils.h"
#include "angelscript/include/angelscript.h"

namespace circuit {

using namespace springai;

CMilitaryScript::CMilitaryScript(CScriptManager* scr, CMilitaryManager* mgr)
		: IUnitModuleScript(scr, mgr)
{
	asIScriptEngine* engine = script->GetEngine();
	int r = engine->RegisterObjectType("CMilitaryManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CMilitaryManager aiMilitaryMgr", manager); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMilitaryManager", "IUnitTask@+ DefaultMakeTask(CCircuitUnit@)", asMETHOD(CMilitaryManager, DefaultMakeTask), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMilitaryManager", "void DefaultMakeDefence(int, const AIFloat3& in)", asMETHOD(CMilitaryManager, DefaultMakeDefence), asCALL_THISCALL); ASSERT(r >= 0);
}

CMilitaryScript::~CMilitaryScript()
{
}

void CMilitaryScript::Init()
{
	asIScriptModule* mod = script->GetEngine()->GetModule("main");
	int r = mod->SetDefaultNamespace("Military"); ASSERT(r >= 0);
	InitModule(mod);
	militaryInfo.makeDefence = script->GetFunc(mod, "void AiMakeDefence(int, const AIFloat3& in)");
	militaryInfo.isAirValid = script->GetFunc(mod, "bool AiIsAirValid()");
}

void CMilitaryScript::MakeDefence(int cluster, const AIFloat3& pos)
{
	if (militaryInfo.makeDefence == nullptr) {
		static_cast<CMilitaryManager*>(manager)->DefaultMakeDefence(cluster, pos);
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(militaryInfo.makeDefence);
	ctx->SetArgDWord(0, cluster);
	ctx->SetArgAddress(1, &const_cast<AIFloat3&>(pos));
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

bool CMilitaryScript::IsAirValid()
{
	if (militaryInfo.isAirValid == nullptr) {
		return true;
	}
	asIScriptContext* ctx = script->PrepareContext(militaryInfo.isAirValid);
	bool result = script->Exec(ctx) ? ctx->GetReturnByte() : true;
	script->ReturnContext(ctx);
	return result;
}

} // namespace circuit
