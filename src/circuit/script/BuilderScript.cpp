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

static void ConstructSResourceVal(SResource* mem, float m, float e)
{
	new(mem) SResource{m, e};
}

CBuilderScript::CBuilderScript(CScriptManager* scr, CBuilderManager* mgr)
		: IUnitModuleScript(scr, mgr)
{
	asIScriptEngine* engine = script->GetEngine();

	int r = engine->RegisterObjectType("SResource", sizeof(SResource), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<SResource>()); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("SResource", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTION(ConstructSResourceVal), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SResource", "float metal", asOFFSET(SResource, metal)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SResource", "float energy", asOFFSET(SResource, energy)); ASSERT(r >= 0);

	r = engine->RegisterObjectType("SBuildTask", sizeof(TaskB::SBuildTask), asOBJ_VALUE | asOBJ_POD); ASSERT(r >= 0);
	static_assert(sizeof(TaskB::SBuildTask::type) == sizeof(char), "IBuilderTask::BuildType is not uint8!");
	r = engine->RegisterObjectProperty("SBuildTask", "uint8 type", asOFFSET(TaskB::SBuildTask, type)); ASSERT(r >= 0);  // Task::BuildType
	static_assert(sizeof(TaskB::SBuildTask::priority) == sizeof(char), "IBuilderTask::Priority is not uint8!");
	r = engine->RegisterObjectProperty("SBuildTask", "uint8 priority", asOFFSET(TaskB::SBuildTask, priority)); ASSERT(r >= 0);  // Task::Priority
	r = engine->RegisterObjectProperty("SBuildTask", "CCircuitDef@ buildDef", asOFFSET(TaskB::SBuildTask, buildDef)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "AIFloat3 position", asOFFSET(TaskB::SBuildTask, position)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "SResource cost", asOFFSET(TaskB::SBuildTask, cost)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "CCircuitDef@ reprDef", asOFFSET(TaskB::SBuildTask, ref.reprDef)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "CCircuitUnit@ target", asOFFSET(TaskB::SBuildTask, ref.target)); ASSERT(r >= 0);
//	r = engine->RegisterObjectProperty("SBuildTask", "IGridLink@ link", asOFFSET(TaskB::SBuildTask, ref.link)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "int pointId", asOFFSET(TaskB::SBuildTask, i.pointId)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "int spotId", asOFFSET(TaskB::SBuildTask, i.spotId)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "float shake", asOFFSET(TaskB::SBuildTask, f.shake)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "float radius", asOFFSET(TaskB::SBuildTask, f.radius)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "bool isPlop", asOFFSET(TaskB::SBuildTask, b.isPlop)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "bool isMetal", asOFFSET(TaskB::SBuildTask, b.isMetal)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "bool isActive", asOFFSET(TaskB::SBuildTask, isActive)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SBuildTask", "int timeout", asOFFSET(TaskB::SBuildTask, timeout)); ASSERT(r >= 0);

	r = engine->RegisterObjectType("SServBTask", sizeof(TaskB::SServBTask), asOBJ_VALUE | asOBJ_POD); ASSERT(r >= 0);
	static_assert(sizeof(TaskB::SServBTask::type) == sizeof(char), "IBuilderTask::BuildType is not uint8!");
	r = engine->RegisterObjectProperty("SServBTask", "uint8 type", asOFFSET(TaskB::SServBTask, type)); ASSERT(r >= 0);  // Task::BuildType
	static_assert(sizeof(TaskB::SServBTask::priority) == sizeof(char), "IBuilderTask::Priority is not uint8!");
	r = engine->RegisterObjectProperty("SServBTask", "uint8 priority", asOFFSET(TaskB::SServBTask, priority)); ASSERT(r >= 0);  // Task::Priority
	r = engine->RegisterObjectProperty("SServBTask", "AIFloat3 position", asOFFSET(TaskB::SServBTask, position)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SServBTask", "CCircuitUnit@ target", asOFFSET(TaskB::SServBTask, target)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SServBTask", "float powerMod", asOFFSET(TaskB::SServBTask, powerMod)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SServBTask", "bool isInterrupt", asOFFSET(TaskB::SServBTask, isInterrupt)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SServBTask", "int timeout", asOFFSET(TaskB::SServBTask, timeout)); ASSERT(r >= 0);

	r = engine->RegisterObjectType("CBuilderManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CBuilderManager aiBuilderMgr", manager); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CBuilderManager", "IUnitTask@+ DefaultMakeTask(CCircuitUnit@)", asMETHOD(CBuilderManager, DefaultMakeTask), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CBuilderManager", "IUnitTask@+ Enqueue(const SBuildTask& in)", asMETHODPR(CBuilderManager, Enqueue, (const TaskB::SBuildTask&), IBuilderTask*), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CBuilderManager", "IUnitTask@+ Enqueue(const SServBTask& in)", asMETHODPR(CBuilderManager, Enqueue, (const TaskB::SServBTask&), IUnitTask*), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CBuilderManager", "IUnitTask@+ EnqueueRetreat()", asMETHOD(CBuilderManager, EnqueueRetreat), asCALL_THISCALL); ASSERT(r >= 0);
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
