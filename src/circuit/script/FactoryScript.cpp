/*
 * FactoryScript.cpp
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#include "script/FactoryScript.h"
#include "script/ScriptManager.h"
#include "module/FactoryManager.h"
#include "util/Utils.h"
#include "angelscript/include/angelscript.h"

namespace circuit {

using namespace springai;

CFactoryScript::CFactoryScript(CScriptManager* scr, CFactoryManager* mgr)
		: IUnitModuleScript(scr, mgr)
{
	asIScriptEngine* engine = script->GetEngine();

	int r = engine->RegisterObjectType("SRecruitTask", sizeof(TaskS::SRecruitTask), asOBJ_VALUE | asOBJ_POD); ASSERT(r >= 0);
	static_assert(sizeof(TaskS::SRecruitTask::type) == sizeof(char), "CRecruitTask::RecruitType is not uint8!");
	r = engine->RegisterObjectProperty("SRecruitTask", "uint8 type", asOFFSET(TaskS::SRecruitTask, type)); ASSERT(r >= 0);  // Task::RecruitType
	static_assert(sizeof(TaskS::SRecruitTask::priority) == sizeof(char), "IBuilderTask::Priority is not uint8!");
	r = engine->RegisterObjectProperty("SRecruitTask", "uint8 priority", asOFFSET(TaskS::SRecruitTask, priority)); ASSERT(r >= 0);  // Task::Priority
	r = engine->RegisterObjectProperty("SRecruitTask", "CCircuitDef@ buildDef", asOFFSET(TaskS::SRecruitTask, buildDef)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SRecruitTask", "AIFloat3 position", asOFFSET(TaskS::SRecruitTask, position)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SRecruitTask", "float radius", asOFFSET(TaskS::SRecruitTask, radius)); ASSERT(r >= 0);

	r = engine->RegisterObjectType("SServSTask", sizeof(TaskS::SServSTask), asOBJ_VALUE | asOBJ_POD); ASSERT(r >= 0);
	static_assert(sizeof(TaskS::SServSTask::type) == sizeof(char), "IBuilderTask::BuildType is not uint8!");
	r = engine->RegisterObjectProperty("SServSTask", "uint8 type", asOFFSET(TaskS::SServSTask, type)); ASSERT(r >= 0);  // Task::RecruitType
	static_assert(sizeof(TaskS::SServSTask::priority) == sizeof(char), "IBuilderTask::Priority is not uint8!");
	r = engine->RegisterObjectProperty("SServSTask", "uint8 priority", asOFFSET(TaskS::SServSTask, priority)); ASSERT(r >= 0);  // Task::Priority
	r = engine->RegisterObjectProperty("SServSTask", "AIFloat3 position", asOFFSET(TaskS::SServSTask, position)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SServSTask", "CCircuitUnit@ target", asOFFSET(TaskS::SServSTask, target)); ASSERT(r >= 0);  // FIXME: CAllyUnit*
	r = engine->RegisterObjectProperty("SServSTask", "float radius", asOFFSET(TaskS::SServSTask, radius)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SServSTask", "bool stop", asOFFSET(TaskS::SServSTask, stop)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SServSTask", "int timeout", asOFFSET(TaskS::SServSTask, timeout)); ASSERT(r >= 0);

	r = engine->RegisterObjectType("CFactoryManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CFactoryManager aiFactoryMgr", manager); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "IUnitTask@+ DefaultMakeTask(CCircuitUnit@)", asMETHOD(CFactoryManager, DefaultMakeTask), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "IUnitTask@+ Enqueue(const SRecruitTask& in)", asMETHODPR(CFactoryManager, Enqueue, (const TaskS::SRecruitTask&), CRecruitTask*), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "IUnitTask@+ Enqueue(const SServSTask& in)", asMETHODPR(CFactoryManager, Enqueue, (const TaskS::SServSTask&), IUnitTask*), asCALL_THISCALL); ASSERT(r >= 0);
//	r = engine->RegisterObjectMethod("CFactoryManager", "IUnitTask@+ EnqueueRetreat()", asMETHOD(CFactoryManager, EnqueueRetreat), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "CCircuitDef@ GetRoleDef(const CCircuitDef@, Type)", asMETHOD(CFactoryManager, GetRoleDef), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CFactoryManager", "int GetFactoryCount()", asMETHOD(CFactoryManager, GetFactoryCount), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CFactoryManager", "bool isAssistRequired", asOFFSET(CFactoryManager, isAssistRequired)); ASSERT(r >= 0);
}

CFactoryScript::~CFactoryScript()
{
}

bool CFactoryScript::Init()
{
	asIScriptModule* mod = script->GetEngine()->GetModule(CScriptManager::mainName.c_str());
	int r = mod->SetDefaultNamespace("Factory"); ASSERT(r >= 0);
	InitModule(mod);
	factoryInfo.isSwitchTime = script->GetFunc(mod, "bool AiIsSwitchTime(int)");
	factoryInfo.isSwitchAllowed = script->GetFunc(mod, "bool AiIsSwitchAllowed(CCircuitDef@)");
	return true;
}

bool CFactoryScript::IsSwitchTime(int lastSwitchFrame)
{
	if (factoryInfo.isSwitchTime == nullptr) {
		return false;
	}
	asIScriptContext* ctx = script->PrepareContext(factoryInfo.isSwitchTime);
	ctx->SetArgDWord(0, lastSwitchFrame);
	const bool result = script->Exec(ctx) ? ctx->GetReturnByte() : false;
	script->ReturnContext(ctx);
	return result;
}

bool CFactoryScript::IsSwitchAllowed(CCircuitDef* facDef)
{
	if (factoryInfo.isSwitchAllowed == nullptr) {
		return true;
	}
	asIScriptContext* ctx = script->PrepareContext(factoryInfo.isSwitchAllowed);
	ctx->SetArgObject(0, facDef);
	const bool result = script->Exec(ctx) ? ctx->GetReturnByte() : false;
	script->ReturnContext(ctx);
	return result;
}

} // namespace circuit
