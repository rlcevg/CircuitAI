/*
 * InitScript.cpp
 *
 *  Created on: May 13, 2020
 *      Author: rlcevg
 */

#include "script/InitScript.h"
#include "script/ScriptManager.h"
#include "script/RefCounter.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "task/builder/BuilderTask.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/MaskHandler.h"
#include "util/Utils.h"

#include "angelscript/include/angelscript.h"
#include "angelscript/add_on/scriptarray/scriptarray.h"
#include "angelscript/add_on/scriptdictionary/scriptdictionary.h"

#include "Log.h"
#include "Drawer.h"
#include "Game.h"

namespace circuit {

using namespace springai;

static void ConstructAIFloat3(AIFloat3* mem)
{
	new(mem) AIFloat3();
}

static void ConstructCopyAIFloat3(AIFloat3* mem, const AIFloat3& o)
{
	new(mem) AIFloat3(o);
}

static void ConstructTypeMask(CMaskHandler::TypeMask* mem)
{
	new(mem) CMaskHandler::TypeMask();
}

static void ConstructCopyTypeMask(CMaskHandler::TypeMask* mem, const CMaskHandler::TypeMask& o)
{
	new(mem) CMaskHandler::TypeMask(o);
}

static CCircuitDef* CCircuitAI_GetCircuitDef(CCircuitAI* circuit, const std::string& name)
{
	return circuit->GetCircuitDef(name.c_str());
}

static const std::string CCircuitDef_GetName(CCircuitDef* cdef)
{
	return cdef->GetDef()->GetName();
}

CInitScript::CInitScript(CScriptManager* scr, CCircuitAI* ai)
		: IScript(scr)
		, circuit(ai)
{
	asIScriptEngine* engine = script->GetEngine();

	// RegisterSpringai
	int r = engine->RegisterObjectType("AIFloat3", sizeof(AIFloat3), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<AIFloat3>()); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("AIFloat3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructAIFloat3), asCALL_CDECL_OBJLAST); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("AIFloat3", asBEHAVE_CONSTRUCT, "void f(const AIFloat3& in)", asFUNCTION(ConstructCopyAIFloat3), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("AIFloat3", "float x", asOFFSET(AIFloat3, x)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("AIFloat3", "float y", asOFFSET(AIFloat3, y)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("AIFloat3", "float z", asOFFSET(AIFloat3, z)); ASSERT(r >= 0);

	// RegisterUtils
	r = engine->RegisterGlobalFunction("void aiLog(const string& in)", asMETHOD(CInitScript, Log), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void aiAddPoint(const AIFloat3& in, const string& in)", asMETHOD(CInitScript, AddPoint), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void aiDelPoint(const AIFloat3& in)", asMETHOD(CInitScript, DelPoint), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int aiDice(const array<float>@+)", asMETHOD(CInitScript, Dice), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int aiMax(int, int)", asMETHODPR(CInitScript, Max<int>, (int, int) const, int), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float aiMax(float, float)", asMETHODPR(CInitScript, Max<float>, (float, float) const, float), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);

	// RegisterCircuitAI
	r = engine->RegisterObjectType("CCircuitAI", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CCircuitAI ai", circuit); ASSERT(r >= 0);

	r = engine->RegisterObjectType("CCircuitDef", 0, asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectType("CCircuitUnit", 0, asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectType("IUnitTask", 0, asOBJ_REF); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("IUnitTask", asBEHAVE_ADDREF, "void f()", asMETHODPR(IRefCounter, AddRef, (), int), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("IUnitTask", asBEHAVE_RELEASE, "void f()", asMETHODPR(IRefCounter, Release, (), int), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IUnitTask", "int GetRefCount()", asMETHODPR(IRefCounter, GetRefCount, (), int), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IUnitTask", "int GetType()", asMETHODPR(IUnitTask, GetType, () const, IUnitTask::Type), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IUnitTask", "const AIFloat3& GetBuildPos()", asMETHODPR(IBuilderTask, GetPosition, () const, const AIFloat3&), asCALL_THISCALL); ASSERT(r >= 0);

	r = engine->RegisterObjectMethod("CCircuitAI", "int GetLastFrame() const", asMETHOD(CCircuitAI, GetLastFrame), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitAI", "CCircuitDef@ GetCircuitDef(const string& in)", asFUNCTION(CCircuitAI_GetCircuitDef), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);

	r = engine->RegisterObjectType("TypeMask", sizeof(CMaskHandler::TypeMask), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CMaskHandler::TypeMask>()); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("TypeMask", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructTypeMask), asCALL_CDECL_OBJLAST); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("TypeMask", asBEHAVE_CONSTRUCT, "void f(const TypeMask& in)", asFUNCTION(ConstructCopyTypeMask), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterTypedef("Type", "int"); ASSERT(r >= 0);
	r = engine->RegisterTypedef("Mask", "uint"); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("TypeMask", "Type type", asOFFSET(CMaskHandler::TypeMask, type)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("TypeMask", "Mask mask", asOFFSET(CMaskHandler::TypeMask, mask)); ASSERT(r >= 0);

	CMaskHandler* roleMasker = &circuit->GetGameAttribute()->GetRoleMasker();
	r = engine->RegisterObjectType("CMaskHandler", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CMaskHandler aiRoleMasker", roleMasker); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMaskHandler", "TypeMask GetTypeMask(const string& in)", asMETHOD(CMaskHandler, GetTypeMask), asCALL_THISCALL); ASSERT(r >= 0);

	r = engine->RegisterTypedef("Id", "int"); ASSERT(r >= 0);

	r = engine->RegisterObjectMethod("CCircuitDef", "bool IsRoleAny(Mask) const", asMETHOD(CCircuitDef, IsRoleAny), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "const string GetName() const", asFUNCTION(CCircuitDef_GetName), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "Id GetId() const", asMETHOD(CCircuitDef, GetId), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "bool IsAvailable(int)", asMETHODPR(CCircuitDef, IsAvailable, (int) const, bool), asCALL_THISCALL); ASSERT(r >= 0);

	r = engine->RegisterObjectMethod("CCircuitUnit", "Id GetId() const", asMETHODPR(CCircuitUnit, GetId, () const, ICoreUnit::Id), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitUnit", "CCircuitDef@ GetCircuitDef() const", asMETHODPR(CCircuitUnit, GetCircuitDef, () const, CCircuitDef*), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitUnit", "const AIFloat3& GetPos(int)", asMETHODPR(CCircuitUnit, GetPos, (int), const AIFloat3&), asCALL_THISCALL); ASSERT(r >= 0);
}

CInitScript::~CInitScript()
{
}

void CInitScript::Init()
{
	script->Load("init", "init.as");
	asIScriptModule* mod = script->GetEngine()->GetModule("init");
	int r = mod->SetDefaultNamespace("Init"); ASSERT(r >= 0);
	info.init = script->GetFunc(mod, "void Init(dictionary@)");

	if (info.init == nullptr) {
		return;
	}

	CScriptDictionary* dict = CScriptDictionary::Create(script->GetEngine());

	asIScriptContext* ctx = script->PrepareContext(info.init);
	ctx->SetArgObject(0, dict);
	script->Exec(ctx);
	script->ReturnContext(ctx);

	CScriptDictionary* catDict;
	if (dict->Get("category", &catDict, dict->GetTypeId("category"))) {
		Game* game = circuit->GetGame();
		std::array<std::pair<std::string, int*>, 5> cats = {
			std::make_pair("air",   &circuit->airCategory),
			std::make_pair("land",  &circuit->landCategory),
			std::make_pair("water", &circuit->waterCategory),
			std::make_pair("bad",   &circuit->badCategory),
			std::make_pair("good",  &circuit->goodCategory)
		};
		for (const auto& kv : cats) {
			std::string value;
			if (catDict->Get(kv.first, &value, catDict->GetTypeId(kv.first))) {
				*kv.second = game->GetCategoriesFlag(value.c_str());
			}
		}
	}
	catDict->Release();

	dict->Release();
}

void CInitScript::RegisterMgr()
{
	asIScriptEngine* engine = script->GetEngine();

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	int r = engine->RegisterObjectType("CTerrainManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CTerrainManager aiTerrainMgr", terrainMgr); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int GetTerrainWidth()", asFUNCTION(CTerrainManager::GetTerrainWidth), asCALL_CDECL); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int GetTerrainHeight()", asFUNCTION(CTerrainManager::GetTerrainHeight), asCALL_CDECL); ASSERT(r >= 0);

	CSetupManager* setupMgr = circuit->GetSetupManager();
	r = engine->RegisterObjectType("CSetupManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CSetupManager aiSetupMgr", setupMgr); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CSetupManager", "CCircuitDef@ GetCommChoice() const", asMETHOD(CSetupManager, GetCommChoice), asCALL_THISCALL); ASSERT(r >= 0);

	CEnemyManager* enemyMgr = circuit->GetEnemyManager();
	r = engine->RegisterObjectType("CEnemyManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CEnemyManager aiEnemyMgr", enemyMgr); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CEnemyManager", "float GetEnemyThreat(Type) const", asMETHODPR(CEnemyManager, GetEnemyThreat, (CCircuitDef::RoleT) const, float), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CEnemyManager", "float GetMobileThreat() const", asMETHOD(CEnemyManager, GetMobileThreat), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CEnemyManager", "float GetEnemyCost(Type) const", asMETHOD(CEnemyManager, GetEnemyCost), asCALL_THISCALL); ASSERT(r >= 0);
}

void CInitScript::Log(const std::string& msg) const
{
	circuit->LOG("%s", msg.c_str());
}

void CInitScript::AddPoint(const AIFloat3& pos, const std::string& msg) const
{
	circuit->GetDrawer()->AddPoint(pos, msg.c_str());
}

void CInitScript::DelPoint(const AIFloat3& pos) const
{
	circuit->GetDrawer()->DeletePointsAndLines(pos);
}

int CInitScript::Dice(const CScriptArray* array) const
{
	float magnitude = 0.f;
	for (asUINT i = 0; i < array->GetSize(); ++i) {
		magnitude += *static_cast<const float*>(array->At(i));
	}
	float dice = (float)rand() / RAND_MAX * magnitude;
	for (asUINT i = 0; i < array->GetSize(); ++i) {
		dice -= *static_cast<const float*>(array->At(i));
		if (dice < 0.f) {
			return i;
		}
	}
	return -1;
}

} // namespace circuit
