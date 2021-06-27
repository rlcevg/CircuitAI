/*
 * EconomyScript.cpp
 *
 *  Created on: Apr 19, 2019
 *      Author: rlcevg
 */

#include "script/EconomyScript.h"
#include "script/ScriptManager.h"
#include "module/EconomyManager.h"
#include "util/Utils.h"
#include "angelscript/include/angelscript.h"

namespace circuit {

using namespace springai;

CEconomyScript::CEconomyScript(CScriptManager* scr, CEconomyManager* mgr)
		: IModuleScript(scr, mgr)
{
	asIScriptEngine* engine = script->GetEngine();
	int r = engine->RegisterObjectType("CEconomyManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CEconomyManager aiEconomyMgr", manager); ASSERT(r >= 0);
//	r = engine->RegisterObjectType("SResourceInfo", sizeof(CEconomyManager::SResourceInfo), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CEconomyManager::SResourceInfo>()); ASSERT(r >= 0);
	r = engine->RegisterObjectType("SResourceInfo", sizeof(CEconomyManager::SResourceInfo), asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SResourceInfo", "const float current", asOFFSET(CEconomyManager::SResourceInfo, current)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SResourceInfo", "const float storage", asOFFSET(CEconomyManager::SResourceInfo, storage)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SResourceInfo", "const float pull", asOFFSET(CEconomyManager::SResourceInfo, pull)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SResourceInfo", "const float income", asOFFSET(CEconomyManager::SResourceInfo, income)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "const SResourceInfo metal", asOFFSET(CEconomyManager, metal)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "const SResourceInfo energy", asOFFSET(CEconomyManager, energy)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "bool isMetalEmpty", asOFFSET(CEconomyManager, isMetalEmpty)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "bool isMetalFull", asOFFSET(CEconomyManager, isMetalFull)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "bool isEnergyStalling", asOFFSET(CEconomyManager, isEnergyStalling)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "bool isEnergyEmpty", asOFFSET(CEconomyManager, isEnergyEmpty)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "bool isEnergyFull", asOFFSET(CEconomyManager, isEnergyFull)); ASSERT(r >= 0);
}

CEconomyScript::~CEconomyScript()
{
}

void CEconomyScript::Init()
{
	asIScriptModule* mod = script->GetEngine()->GetModule(CScriptManager::mainName.c_str());
	int r = mod->SetDefaultNamespace("Economy"); ASSERT(r >= 0);
	info.openStrategy = script->GetFunc(mod, "void AiOpenStrategy(const CCircuitDef@, const AIFloat3& in)");
	info.updateEconomy = script->GetFunc(mod, "void AiUpdateEconomy()");
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

void CEconomyScript::UpdateEconomy()
{
	if (info.updateEconomy == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(info.updateEconomy);
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

} // namespace circuit
