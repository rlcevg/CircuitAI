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
	r = engine->RegisterObjectProperty("CEconomyManager", "float reclConvertEff", asOFFSET(CEconomyManager, reclConvertEff)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEconomyManager", "float reclEnergyEff", asOFFSET(CEconomyManager, reclEnergyEff)); ASSERT(r >= 0);
}

CEconomyScript::~CEconomyScript()
{
}

bool CEconomyScript::Init()
{
	asIScriptModule* mod = script->GetEngine()->GetModule(CScriptManager::mainName.c_str());
	int r = mod->SetDefaultNamespace("Economy"); ASSERT(r >= 0);
	economyInfo.updateEconomy = script->GetFunc(mod, "void AiUpdateEconomy()");
	return true;
}

void CEconomyScript::UpdateEconomy()
{
	if (economyInfo.updateEconomy == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(economyInfo.updateEconomy);
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

} // namespace circuit
