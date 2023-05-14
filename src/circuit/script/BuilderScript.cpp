/*
 * BuilderScript.cpp
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#include "script/BuilderScript.h"
#include "script/ScriptManager.h"
#include "module/BuilderManager.h"
#include "util/Utils.h"
#include "angelscript/include/angelscript.h"

namespace circuit {

using namespace springai;

CBuilderScript::CBuilderScript(CScriptManager* scr, CBuilderManager* mgr)
		: IUnitModuleScript(scr, mgr)
{
	asIScriptEngine* engine = script->GetEngine();
	int r = engine->RegisterObjectType("CBuilderManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CBuilderManager aiBuilderMgr", manager); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CBuilderManager", "IUnitTask@+ DefaultMakeTask(CCircuitUnit@)", asMETHOD(CBuilderManager, DefaultMakeTask), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CBuilderManager", "uint GetWorkerCount() const", asMETHOD(CBuilderManager, GetWorkerCount), asCALL_THISCALL); ASSERT(r >= 0);
}

CBuilderScript::~CBuilderScript()
{
}

bool CBuilderScript::Init()
{
	asIScriptModule* mod = script->GetEngine()->GetModule(CScriptManager::mainName.c_str());
	int r = mod->SetDefaultNamespace("Builder"); ASSERT(r >= 0);
	InitModule(mod);
	return true;
}

} // namespace circuit
