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
//	r = engine->RegisterObjectType("ResourceInfo", sizeof(CEconomyManager::ResourceInfo), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CEconomyManager::ResourceInfo>()); ASSERT(r >= 0);
	r = engine->RegisterObjectType("ResourceInfo", sizeof(CEconomyManager::ResourceInfo), asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("ResourceInfo", "float current", asOFFSET(CEconomyManager::ResourceInfo, current)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("ResourceInfo", "float storage", asOFFSET(CEconomyManager::ResourceInfo, storage)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("ResourceInfo", "float pull", asOFFSET(CEconomyManager::ResourceInfo, pull)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("ResourceInfo", "float income", asOFFSET(CEconomyManager::ResourceInfo, income)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "ResourceInfo metal", asOFFSET(CEconomyManager, metal)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "ResourceInfo energy", asOFFSET(CEconomyManager, energy)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "bool isMetalEmpty", asOFFSET(CEconomyManager, isMetalEmpty)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "bool isMetalFull", asOFFSET(CEconomyManager, isMetalFull)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "bool isEnergyStalling", asOFFSET(CEconomyManager, isEnergyStalling)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "bool isEnergyEmpty", asOFFSET(CEconomyManager, isEnergyEmpty)); ASSERT(r >= 0);
}

CEconomyScript::~CEconomyScript()
{
}

void CEconomyScript::Init()
{
	asIScriptModule* mod = script->GetEngine()->GetModule("main");
	info.openStrategy = script->GetFunc(mod, "void OpenStrategy(const CCircuitDef@, const AIFloat3& in)");
	info.updateEconomy = script->GetFunc(mod, "void UpdateEconomy()");
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
