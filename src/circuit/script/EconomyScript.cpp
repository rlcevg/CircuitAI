/*
 * EconomyScript.cpp
 *
 *  Created on: Apr 19, 2019
 *      Author: rlcevg
 */

#include "script/EconomyScript.h"
#include "script/ScriptManager.h"
#include "module/EconomyManager.h"
#include "angelscript/include/angelscript.h"

namespace circuit {

using namespace springai;

CEconomyScript::CEconomyScript(CScriptManager* scr, CEconomyManager* mgr)
		: IModuleScript(scr, mgr)
{
}

CEconomyScript::~CEconomyScript()
{
}

void CEconomyScript::Init()
{
	script->Load("economy", "manager/economy.as");
	asIScriptModule* mod = script->GetEngine()->GetModule("economy");
	info.openStrategy = script->GetFunc(mod, "void OpenStrategy(const CCircuitDef@, const AIFloat3& in)");
}

void CEconomyScript::OpenStrategy(const CCircuitDef* facDef, const AIFloat3& pos)
{
	if (info.openStrategy == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(info.openStrategy);
	ctx->SetArgObject(0, const_cast<CCircuitDef*>(facDef));
	ctx->SetArgAddress(1, &const_cast<AIFloat3&>(pos));
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

} // namespace circuit
