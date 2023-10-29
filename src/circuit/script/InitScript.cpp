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

#include "spring/SpringMap.h"

#include "Log.h"
#include "Drawer.h"
#include "Game.h"

namespace circuit {

using namespace springai;

CInitScript::SInitInfo::SInitInfo(const SInitInfo& o)
{
	armor = o.armor;
	category = o.category;
	if (profile != nullptr) {
		profile->Release();
	}
	profile = o.profile;
	if (profile != nullptr) {
		profile->AddRef();
	}
}

CInitScript::SInitInfo::~SInitInfo()
{
	if (profile != nullptr) {
		profile->Release();
	}
}

static void AddAirArmor(CCircuitDef::SArmorInfo* mem, int type)
{
	mem->airTypes.push_back(type);
}

static void AddSurfaceArmor(CCircuitDef::SArmorInfo* mem, int type)
{
	mem->surfTypes.push_back(type);
}

static void AddWaterArmor(CCircuitDef::SArmorInfo* mem, int type)
{
	mem->waterTypes.push_back(type);
}

static void ConstructAIFloat3(AIFloat3* mem)
{
	new(mem) AIFloat3();
}

static void ConstructCopyAIFloat3(AIFloat3* mem, const AIFloat3& o)
{
	new(mem) AIFloat3(o);
}

static void ConstructAIFloat3Val(AIFloat3* mem, float x, float y, float z)
{
	new(mem) AIFloat3(x, y, z);
}

static void ConstructSArmorInfo(CCircuitDef::SArmorInfo* mem)
{
	new(mem) CCircuitDef::SArmorInfo();
}

static void ConstructCopySArmorInfo(CCircuitDef::SArmorInfo* mem, const CCircuitDef::SArmorInfo& o)
{
	new(mem) CCircuitDef::SArmorInfo(o);
}

static void DestructSArmorInfo(CCircuitDef::SArmorInfo *mem)
{
	mem->~SArmorInfo();
}

static CCircuitDef::SArmorInfo& AssignSArmorInfoToSArmorInfo(CCircuitDef::SArmorInfo& mem, const CCircuitDef::SArmorInfo& o)
{
	mem = o;
	return mem;
}

static void ConstructSCategoryInfo(CInitScript::SInitInfo::SCategoryInfo* mem)
{
	new(mem) CInitScript::SInitInfo::SCategoryInfo();
}

static void ConstructCopySCategoryInfo(CInitScript::SInitInfo::SCategoryInfo* mem, const CInitScript::SInitInfo::SCategoryInfo& o)
{
	new(mem) CInitScript::SInitInfo::SCategoryInfo(o);
}

static void DestructSCategoryInfo(CInitScript::SInitInfo::SCategoryInfo *mem)
{
	mem->~SCategoryInfo();
}

static CInitScript::SInitInfo::SCategoryInfo& AssignSCategoryInfoToSCategoryInfo(CInitScript::SInitInfo::SCategoryInfo& mem, const CInitScript::SInitInfo::SCategoryInfo& o)
{
	mem = o;
	return mem;
}

static void ConstructSInitInfo(CInitScript::SInitInfo* mem)
{
	new(mem) CInitScript::SInitInfo();
}

static void ConstructCopySInitInfo(CInitScript::SInitInfo* mem, const CInitScript::SInitInfo& o)
{
	new(mem) CInitScript::SInitInfo(o);
}

static void DestructSInitInfo(CInitScript::SInitInfo *mem)
{
	mem->~SInitInfo();
}

static CCircuitDef* CCircuitAI_GetCircuitDef(CCircuitAI* circuit, const std::string& name)
{
	return circuit->GetCircuitDef(name.c_str());
}

static std::string CCircuitAI_GetMapName(CCircuitAI* circuit)
{
	return circuit->GetMap()->GetName();
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
	r = engine->RegisterObjectBehaviour("AIFloat3", asBEHAVE_CONSTRUCT, "void f(float, float, float)", asFUNCTION(ConstructAIFloat3Val), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("AIFloat3", "float x", asOFFSET(AIFloat3, x)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("AIFloat3", "float y", asOFFSET(AIFloat3, y)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("AIFloat3", "float z", asOFFSET(AIFloat3, z)); ASSERT(r >= 0);

	// RegisterUtils
	r = engine->RegisterGlobalFunction("void AiLog(const string& in)", asMETHOD(CInitScript, Log), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void AiAddPoint(const AIFloat3& in, const string& in)", asMETHOD(CInitScript, AddPoint), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void AiDelPoint(const AIFloat3& in)", asMETHOD(CInitScript, DelPoint), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("void AiPause(bool, const string& in)", asMETHOD(CInitScript, Pause), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int AiDice(const array<float>@+)", asMETHOD(CInitScript, Dice), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int AiMin(int, int)", asMETHODPR(CInitScript, Min<int>, (int, int) const, int), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float AiMin(float, float)", asMETHODPR(CInitScript, Min<float>, (float, float) const, float), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int AiMax(int, int)", asMETHODPR(CInitScript, Max<int>, (int, int) const, int), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float AiMax(float, float)", asMETHODPR(CInitScript, Max<float>, (float, float) const, float), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int AiRandom(int, int)", asMETHOD(CInitScript, Random), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);

	r = engine->RegisterObjectType("IStream", sizeof(std::istream), asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectType("OStream", sizeof(std::ostream), asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IStream", "IStream& opShr(bool& out)", asFUNCTION(utils::binary_read<bool>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IStream", "IStream& opShr(int8& out)", asFUNCTION(utils::binary_read<int8_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IStream", "IStream& opShr(int16& out)", asFUNCTION(utils::binary_read<int16_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IStream", "IStream& opShr(int& out)", asFUNCTION(utils::binary_read<int32_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IStream", "IStream& opShr(int64& out)", asFUNCTION(utils::binary_read<int64_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IStream", "IStream& opShr(uint8& out)", asFUNCTION(utils::binary_read<uint8_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IStream", "IStream& opShr(uint16& out)", asFUNCTION(utils::binary_read<uint16_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IStream", "IStream& opShr(uint& out)", asFUNCTION(utils::binary_read<uint32_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IStream", "IStream& opShr(uint64& out)", asFUNCTION(utils::binary_read<uint64_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IStream", "IStream& opShr(float& out)", asFUNCTION(utils::binary_read<float>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IStream", "IStream& opShr(double& out)", asFUNCTION(utils::binary_read<double>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("OStream", "OStream& opShl(const bool& in)", asFUNCTION(utils::binary_write<bool>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("OStream", "OStream& opShl(const int8& in)", asFUNCTION(utils::binary_write<int8_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("OStream", "OStream& opShl(const int16& in)", asFUNCTION(utils::binary_write<int16_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("OStream", "OStream& opShl(const int& in)", asFUNCTION(utils::binary_write<int32_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("OStream", "OStream& opShl(const int64& in)", asFUNCTION(utils::binary_write<int64_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("OStream", "OStream& opShl(const uint8& in)", asFUNCTION(utils::binary_write<uint8_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("OStream", "OStream& opShl(const uint16& in)", asFUNCTION(utils::binary_write<uint16_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("OStream", "OStream& opShl(const uint& in)", asFUNCTION(utils::binary_write<uint32_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("OStream", "OStream& opShl(const uint64& in)", asFUNCTION(utils::binary_write<uint64_t>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("OStream", "OStream& opShl(const float& in)", asFUNCTION(utils::binary_write<float>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("OStream", "OStream& opShl(const double& in)", asFUNCTION(utils::binary_write<double>), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);

	// RegisterCircuitAI
	r = engine->RegisterTypedef("Id", "int"); ASSERT(r >= 0);

	r = engine->RegisterObjectType("TypeMask", sizeof(CMaskHandler::TypeMask), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CMaskHandler::TypeMask>()); ASSERT(r >= 0);
	r = engine->RegisterTypedef("Type", "int"); ASSERT(r >= 0);
	r = engine->RegisterTypedef("Mask", "uint"); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("TypeMask", "Type type", asOFFSET(CMaskHandler::TypeMask, type)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("TypeMask", "Mask mask", asOFFSET(CMaskHandler::TypeMask, mask)); ASSERT(r >= 0);

//	r = engine->SetDefaultNamespace("Task"); ASSERT(r >= 0);
//	r = engine->RegisterEnum("RecruitType"); ASSERT(r >= 0);
//	r = engine->RegisterEnumValue("RecruitType", "BUILDPOWER", static_cast<int>(CRecruitTask::RecruitType::BUILDPOWER)); ASSERT(r >= 0);
//	r = engine->RegisterEnumValue("RecruitType", "FIREPOWER", static_cast<int>(CRecruitTask::RecruitType::FIREPOWER)); ASSERT(r >= 0);
//	r = engine->SetDefaultNamespace(""); ASSERT(r >= 0);

	r = engine->RegisterObjectType("CCircuitAI", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CCircuitAI ai", circuit); ASSERT(r >= 0);

	r = engine->RegisterObjectType("CCircuitDef", 0, asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectType("CCircuitUnit", 0, asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectType("IUnitTask", 0, asOBJ_REF); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("IUnitTask", asBEHAVE_ADDREF, "void f()", asMETHODPR(IRefCounter, AddRef, (), int), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("IUnitTask", asBEHAVE_RELEASE, "void f()", asMETHODPR(IRefCounter, Release, (), int), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IUnitTask", "int GetRefCount() const", asMETHODPR(IRefCounter, GetRefCount, () const, int), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IUnitTask", "Type GetType() const", asMETHODPR(IUnitTask, GetType, () const, IUnitTask::Type), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IUnitTask", "Type GetBuildType() const", asMETHODPR(IBuilderTask, GetBuildType, () const, IBuilderTask::BuildType), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IUnitTask", "const AIFloat3& GetBuildPos() const", asMETHODPR(IBuilderTask, GetPosition, () const, const AIFloat3&), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("IUnitTask", "CCircuitDef@ GetBuildDef() const", asMETHODPR(IBuilderTask, GetBuildDef, () const, CCircuitDef*), asCALL_THISCALL); ASSERT(r >= 0);

	r = engine->RegisterObjectProperty("CCircuitAI", "const int frame", asOFFSET(CCircuitAI, lastFrame)); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitAI", "CCircuitDef@ GetCircuitDef(const string& in)", asFUNCTION(CCircuitAI_GetCircuitDef), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitAI", "CCircuitDef@ GetCircuitDef(Id)", asMETHODPR(CCircuitAI, GetCircuitDef, (CCircuitDef::Id), CCircuitDef*), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitAI", "int GetDefCount() const", asMETHOD(CCircuitAI, GetDefCount), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitAI", "CCircuitUnit@ GetTeamUnit(Id)", asMETHOD(CCircuitAI, GetTeamUnit), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitAI", "string GetMapName() const", asFUNCTION(CCircuitAI_GetMapName), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitAI", "int GetEnemyTeamSize() const", asMETHOD(CCircuitAI, GetEnemyTeamSize), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitAI", "bool IsLoadSave() const", asMETHOD(CCircuitAI, IsLoadSave), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitAI", "Type GetBindedRole(Type) const", asMETHOD(CCircuitAI, GetBindedRole), asCALL_THISCALL); ASSERT(r >= 0);

	CMaskHandler* sideMasker = &circuit->GetGameAttribute()->GetSideMasker();
	CMaskHandler* roleMasker = &circuit->GetGameAttribute()->GetRoleMasker();
	CMaskHandler* attrMasker = &circuit->GetGameAttribute()->GetAttrMasker();
	r = engine->RegisterObjectType("CMaskHandler", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CMaskHandler aiSideMasker", sideMasker); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CMaskHandler aiRoleMasker", roleMasker); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CMaskHandler aiAttrMasker", attrMasker); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMaskHandler", "TypeMask GetTypeMask(const string& in)", asMETHOD(CMaskHandler, GetTypeMask), asCALL_THISCALL); ASSERT(r >= 0);

	r = engine->RegisterGlobalFunction("TypeMask AiAddRole(const string& in, Type)", asMETHOD(CInitScript, AddRole), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);

	r = engine->RegisterObjectMethod("CCircuitDef", "void SetMainRole(Type)", asMETHOD(CCircuitDef, SetMainRole), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "Type GetMainRole() const", asMETHOD(CCircuitDef, GetMainRole), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "bool IsRespRoleAny(Mask) const", asMETHOD(CCircuitDef, IsRespRoleAny), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "bool IsRoleAny(Mask) const", asMETHOD(CCircuitDef, IsRoleAny), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "void AddAttribute(Type)", asMETHOD(CCircuitDef, AddAttribute), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "void DelAttribute(Type)", asMETHOD(CCircuitDef, DelAttribute), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "void TglAttribute(Type)", asMETHOD(CCircuitDef, TglAttribute), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "bool IsAttrAny(Mask) const", asMETHOD(CCircuitDef, IsAttrAny), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "const string GetName() const", asFUNCTION(CCircuitDef_GetName), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CCircuitDef", "const Id id", asOFFSET(CCircuitDef, id)); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "bool IsAvailable(int)", asMETHODPR(CCircuitDef, IsAvailable, (int) const, bool), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CCircuitDef", "const int count", asOFFSET(CCircuitDef, count)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CCircuitDef", "const float costM", asOFFSET(CCircuitDef, costM)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CCircuitDef", "const float costE", asOFFSET(CCircuitDef, costE)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CCircuitDef", "const float threat", asOFFSET(CCircuitDef, defThreat)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CCircuitDef", "const float power", asOFFSET(CCircuitDef, power)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CCircuitDef", "const float minRange", asOFFSET(CCircuitDef, minRange)); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "float GetAirThreat() const", asMETHOD(CCircuitDef, GetAirThreat), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "float GetSurfThreat() const", asMETHOD(CCircuitDef, GetSurfThreat), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "float GetWaterThreat() const", asMETHOD(CCircuitDef, GetWaterThreat), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "bool IsAbleToFly() const", asMETHOD(CCircuitDef, IsAbleToFly), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CCircuitDef", "int maxThisUnit", asOFFSET(CCircuitDef, maxThisUnit)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CCircuitDef", "int sinceFrame", asOFFSET(CCircuitDef, sinceFrame)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CCircuitDef", "int cooldown", asOFFSET(CCircuitDef, cooldown)); ASSERT(r >= 0);

	r = engine->RegisterObjectProperty("CCircuitUnit", "const Id id", asOFFSET(CCircuitUnit, id)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CCircuitUnit", "const CCircuitDef@ circuitDef", asOFFSET(CCircuitUnit, circuitDef)); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitUnit", "const AIFloat3& GetPos(int)", asMETHODPR(CCircuitUnit, GetPos, (int), const AIFloat3&), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitUnit", "void AddAttribute(Type)", asMETHOD(CCircuitUnit, AddAttribute), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitUnit", "void DelAttribute(Type)", asMETHOD(CCircuitUnit, DelAttribute), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitUnit", "void TglAttribute(Type)", asMETHOD(CCircuitUnit, TglAttribute), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitUnit", "bool IsAttrAny(Mask) const", asMETHOD(CCircuitUnit, IsAttrAny), asCALL_THISCALL); ASSERT(r >= 0);
}

CInitScript::~CInitScript()
{
}

bool CInitScript::InitConfig(const std::string& profile,
		std::vector<std::string>& outCfgParts, CCircuitDef::SArmorInfo& outArmor)
{
	asIScriptEngine* engine = script->GetEngine();
	// FIXME: asASSERT( refCount == 0 ); at lib/angelscript/source/as_configgroup.cpp:157
	//        on exit
//	int r = engine->BeginConfigGroup(CScriptManager::initName.c_str()); ASSERT(r >= 0);
	int r = engine->RegisterObjectType("SArmorInfo", sizeof(CCircuitDef::SArmorInfo), asOBJ_VALUE | asGetTypeTraits<CCircuitDef::SArmorInfo>()); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("SArmorInfo", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructSArmorInfo), asCALL_CDECL_OBJLAST); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("SArmorInfo", asBEHAVE_CONSTRUCT, "void f(const SArmorInfo& in)", asFUNCTION(ConstructCopySArmorInfo), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("SArmorInfo", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(DestructSArmorInfo), asCALL_CDECL_OBJLAST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("SArmorInfo", "SArmorInfo &opAssign(const SArmorInfo &in)", asFUNCTION(AssignSArmorInfoToSArmorInfo), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
	r = engine->RegisterObjectMethod("SArmorInfo", "void AddAir(int)", asFUNCTION(AddAirArmor), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("SArmorInfo", "void AddSurface(int)", asFUNCTION(AddSurfaceArmor), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("SArmorInfo", "void AddWater(int)", asFUNCTION(AddWaterArmor), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectType("SCategoryInfo", sizeof(SInitInfo::SCategoryInfo), asOBJ_VALUE | asGetTypeTraits<SInitInfo::SCategoryInfo>()); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("SCategoryInfo", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructSCategoryInfo), asCALL_CDECL_OBJLAST); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("SCategoryInfo", asBEHAVE_CONSTRUCT, "void f(const SCategoryInfo& in)", asFUNCTION(ConstructCopySCategoryInfo), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("SCategoryInfo", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(DestructSCategoryInfo), asCALL_CDECL_OBJLAST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("SCategoryInfo", "SCategoryInfo &opAssign(const SCategoryInfo &in)", asFUNCTION(AssignSCategoryInfoToSCategoryInfo), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
	r = engine->RegisterObjectProperty("SCategoryInfo", "string air", asOFFSET(SInitInfo::SCategoryInfo, air)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SCategoryInfo", "string land", asOFFSET(SInitInfo::SCategoryInfo, land)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SCategoryInfo", "string water", asOFFSET(SInitInfo::SCategoryInfo, water)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SCategoryInfo", "string bad", asOFFSET(SInitInfo::SCategoryInfo, bad)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SCategoryInfo", "string good", asOFFSET(SInitInfo::SCategoryInfo, good)); ASSERT(r >= 0);
	r = engine->RegisterObjectType("SInitInfo", sizeof(SInitInfo), asOBJ_VALUE | asGetTypeTraits<SInitInfo>()); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("SInitInfo", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructSInitInfo), asCALL_CDECL_OBJLAST); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("SInitInfo", asBEHAVE_CONSTRUCT, "void f(const SInitInfo& in)", asFUNCTION(ConstructCopySInitInfo), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("SInitInfo", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(DestructSInitInfo), asCALL_CDECL_OBJLAST); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SInitInfo", "SArmorInfo armor", asOFFSET(SInitInfo, armor)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SInitInfo", "SCategoryInfo category", asOFFSET(SInitInfo, category)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("SInitInfo", "array<string>@ profile", asOFFSET(SInitInfo, profile)); ASSERT(r >= 0);
//	r = engine->EndConfigGroup(); ASSERT(r >= 0);

	folderName = profile;
	if (!script->Load(CScriptManager::initName.c_str(), folderName, CScriptManager::initName + ".as")) {
		return false;
	}
	asIScriptModule* mod = script->GetEngine()->GetModule(CScriptManager::initName.c_str());
	r = mod->SetDefaultNamespace("Init"); ASSERT(r >= 0);
	asIScriptFunction* init = script->GetFunc(mod, "SInitInfo AiInit()");

	if (init != nullptr) {
		asIScriptContext* ctx = script->PrepareContext(init);
		SInitInfo* result = script->Exec(ctx) ? (SInitInfo*)ctx->GetReturnObject() : nullptr;
		if (result != nullptr) {
			outArmor = result->armor;
			if (outArmor.airTypes.empty()) {
				outArmor.airTypes.push_back(0);  // default
			}
			if (outArmor.surfTypes.empty()) {
				outArmor.surfTypes.push_back(0);  // default
			}
			if (outArmor.waterTypes.empty()) {
				outArmor.waterTypes.push_back(0);  // default
			}

			Game* game = circuit->GetGame();
			circuit->category.air = game->GetCategoriesFlag(result->category.air.c_str());
			circuit->category.land = game->GetCategoriesFlag(result->category.land.c_str());
			circuit->category.water = game->GetCategoriesFlag(result->category.water.c_str());
			circuit->category.bad = game->GetCategoriesFlag(result->category.bad.c_str());
			circuit->category.good = game->GetCategoriesFlag(result->category.good.c_str());

			if (result->profile != nullptr) {
				for (unsigned j = 0; j < result->profile->GetSize(); ++j) {
					outCfgParts.push_back(*(std::string*)result->profile->At(j));
				}
			}
		}
		// NOTE: Init context shouldn't be used again, hence release
//		ctx->Unprepare();
//		script->ReturnContext(ctx);
		ctx->Release();
	}

	mod->Discard();
	// NOTE: destroys "array<T>". "main" should be registered and loaded first,
	//       then "array<T>" will be in defaultGroup - not viable option.
	//       And re-creating CScriptManager is not worth the effort.
//	r = script->GetEngine()->RemoveConfigGroup(CScriptManager::initName.c_str()); ASSERT(r >= 0);
	return true;
}

bool CInitScript::Init()
{
	if (!script->Load(CScriptManager::mainName.c_str(), folderName, CScriptManager::mainName + ".as")) {
		return false;
	}

	asIScriptModule* mod = script->GetEngine()->GetModule(CScriptManager::mainName.c_str());
	int r = mod->SetDefaultNamespace("Main"); ASSERT(r >= 0);
	asIScriptFunction* main = script->GetFunc(mod, "void AiMain()");
	if (main == nullptr) {
		return false;
	}

	asIScriptContext* ctx = script->PrepareContext(main);
	r = ctx->Prepare(main); ASSERT(r >= 0);
	script->Exec(ctx);
	script->ReturnContext(ctx);
	return true;
}

void CInitScript::RegisterMgr()
{
	asIScriptEngine* engine = script->GetEngine();

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	int r = engine->RegisterObjectType("CTerrainManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CTerrainManager aiTerrainMgr", terrainMgr); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int AiTerrainWidth()", asFUNCTION(CTerrainManager::GetTerrainWidth), asCALL_CDECL); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int AiTerrainHeight()", asFUNCTION(CTerrainManager::GetTerrainHeight), asCALL_CDECL); ASSERT(r >= 0);

	CSetupManager* setupMgr = circuit->GetSetupManager();
	r = engine->RegisterObjectType("CSetupManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CSetupManager aiSetupMgr", setupMgr); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CSetupManager", "const CCircuitDef@ commChoice", asOFFSET(CSetupManager, commChoice)); ASSERT(r >= 0);

	CEnemyManager* enemyMgr = circuit->GetEnemyManager();
	r = engine->RegisterObjectType("CEnemyManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CEnemyManager aiEnemyMgr", enemyMgr); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CEnemyManager", "float GetEnemyThreat(Type) const", asMETHODPR(CEnemyManager, GetEnemyThreat, (CCircuitDef::RoleT) const, float), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("CEnemyManager", "const float mobileThreat", asOFFSET(CEnemyManager, mobileThreat)); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CEnemyManager", "float GetEnemyCost(Type) const", asMETHOD(CEnemyManager, GetEnemyCost), asCALL_THISCALL); ASSERT(r >= 0);
}

CMaskHandler::TypeMask CInitScript::AddRole(const std::string& name, int actAsRole)
{
	CMaskHandler::TypeMask result = circuit->GetGameAttribute()->GetRoleMasker().GetTypeMask(name);
	if (result.type < 0) {
		return result;
	}
	circuit->BindRole(result.type, actAsRole);
	return result;
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

void CInitScript::Pause(bool enable, const std::string& msg) const
{
	circuit->GetGame()->SetPause(enable, msg.c_str());
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
