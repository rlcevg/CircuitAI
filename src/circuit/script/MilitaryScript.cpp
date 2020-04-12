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

CMilitaryScript::CMilitaryScript(CScriptManager* scr, CMilitaryManager* mgr)
		: IScript(scr, mgr)
{
	asIScriptEngine* engine = script->GetEngine();
	int r = engine->RegisterObjectType("CMilitaryManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CMilitaryManager militaryMgr", manager); ASSERT(r >= 0);
}

CMilitaryScript::~CMilitaryScript()
{
}

void CMilitaryScript::Init()
{
	// FIXME: init CEnemyManager elsewhere
	asIScriptEngine* engine = script->GetEngine();
	int r = engine->RegisterObjectType("CEnemyManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CEnemyManager enemyMgr", static_cast<CMilitaryManager*>(manager)->GetCircuit()->GetEnemyManager()); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CEnemyManager", "float GetEnemyThreat(Type) const", asMETHODPR(CEnemyManager, GetEnemyThreat, (CCircuitDef::RoleT) const, float), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CEnemyManager", "float GetMobileThreat() const", asMETHOD(CEnemyManager, GetMobileThreat), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CEnemyManager", "float GetEnemyCost(Type) const", asMETHOD(CEnemyManager, GetEnemyCost), asCALL_THISCALL); ASSERT(r >= 0);

	script->Load("military", "manager/military.as");
	asIScriptModule* mod = script->GetEngine()->GetModule("military");
	info.isAirValid = script->GetFunc(mod, "bool isAirValid()");
}

bool CMilitaryScript::IsAirValid()
{
	if (info.isAirValid == nullptr) {
		return true;
	}
	asIScriptContext* ctx = script->PrepareContext(info.isAirValid);
	bool result = script->Exec(ctx) ? ctx->GetReturnByte() : true;
	script->ReturnContext(ctx);
	return result;
}

} // namespace circuit
