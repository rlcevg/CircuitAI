/*
 * ModuleScript.cpp
 *
 *  Created on: May 13, 2020
 *      Author: rlcevg
 */

#include "script/ModuleScript.h"
#include "script/ScriptManager.h"
#include "util/Utils.h"
#include "angelscript/include/angelscript.h"

namespace circuit {

IModuleScript::IModuleScript(CScriptManager* scr, IModule* mod)
		: IScript(scr)
		, manager(mod)
{
}

IModuleScript::~IModuleScript()
{
}

void IModuleScript::InitModule(asIScriptModule* mod)
{
	mInfo.unitAdded = script->GetFunc(mod, "void AiUnitAdded(CCircuitUnit@, Unit::UseAs)");
	mInfo.unitRemoved = script->GetFunc(mod, "void AiUnitRemoved(CCircuitUnit@, Unit::UseAs)");
	mInfo.load = script->GetFunc(mod, "void AiLoad(IStream&)");
	mInfo.save = script->GetFunc(mod, "void AiSave(OStream&)");
}

void IModuleScript::UnitAdded(CCircuitUnit* unit, IModule::UseAs usage)
{
	if (mInfo.unitAdded == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(mInfo.unitAdded);
	ctx->SetArgObject(0, unit);
	ctx->SetArgDWord(1, static_cast<std::underlying_type<decltype(usage)>::type>(usage));
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

void IModuleScript::UnitRemoved(CCircuitUnit* unit, IModule::UseAs usage)
{
	if (mInfo.unitRemoved == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(mInfo.unitRemoved);
	ctx->SetArgObject(0, unit);
	ctx->SetArgDWord(1, static_cast<std::underlying_type<decltype(usage)>::type>(usage));
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

void IModuleScript::Load(std::istream& is)
{
	if (mInfo.load == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(mInfo.load);
	ctx->SetArgObject(0, &is);
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

void IModuleScript::Save(std::ostream& os) const
{
	if (mInfo.save == nullptr) {
		return;
	}
	asIScriptContext* ctx = script->PrepareContext(mInfo.save);
	ctx->SetArgObject(0, &os);
	script->Exec(ctx);
	script->ReturnContext(ctx);
}

} // namespace circuit
