/*
 * InitScript.cpp
 *
 *  Created on: May 13, 2020
 *      Author: rlcevg
 */

#include "script/InitScript.h"
#include "script/ScriptManager.h"
#include "script/RefCounter.h"
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

static void ConstructTypeMask(CMaskHandler::TypeMask* mem)
{
	new(mem) CMaskHandler::TypeMask();
}

static void ConstructCopyTypeMask(CMaskHandler::TypeMask* mem, const CMaskHandler::TypeMask& o)
{
	new(mem) CMaskHandler::TypeMask(o);
}

CInitScript::CInitScript(CScriptManager* scr, CCircuitAI* ai)
		: script(scr)
		, circuit(ai)
{
	asIScriptEngine* engine = script->GetEngine();
	int r;

	// RegisterUtils
	r = engine->RegisterGlobalFunction("void aiLog(const string& in)", asMETHOD(CInitScript, Log), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int aiDice(const array<float>@+)", asMETHOD(CInitScript, Dice), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int aiMax(int, int)", asMETHODPR(CInitScript, Max<int>, (int, int) const, int), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float aiMax(float, float)", asMETHODPR(CInitScript, Max<float>, (float, float) const, float), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);

	r = engine->RegisterObjectType("TypeMask", sizeof(CMaskHandler::TypeMask), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CMaskHandler::TypeMask>()); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("TypeMask", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructTypeMask), asCALL_CDECL_OBJLAST); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("TypeMask", asBEHAVE_CONSTRUCT, "void f(const TypeMask& in)", asFUNCTION(ConstructCopyTypeMask), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterTypedef("Type", "int"); ASSERT(r >= 0);
	r = engine->RegisterTypedef("Mask", "uint"); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("TypeMask", "Type type", asOFFSET(CMaskHandler::TypeMask, type)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("TypeMask", "Mask mask", asOFFSET(CMaskHandler::TypeMask, mask)); ASSERT(r >= 0);

	CMaskHandler* sideMasker = &circuit->GetGameAttribute()->GetSideMasker();
	CMaskHandler* roleMasker = &circuit->GetGameAttribute()->GetRoleMasker();
	r = engine->RegisterObjectType("CMaskHandler", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CMaskHandler aiSideMasker", sideMasker); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CMaskHandler aiRoleMasker", roleMasker); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMaskHandler", "TypeMask GetTypeMask(const string& in)", asMETHOD(CMaskHandler, GetTypeMask), asCALL_THISCALL); ASSERT(r >= 0);

	r = engine->RegisterGlobalFunction("TypeMask aiAddRole(const string& in, Type)", asMETHOD(CInitScript, AddRole), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);

	r = engine->RegisterTypedef("Id", "int"); ASSERT(r >= 0);
}

CInitScript::~CInitScript()
{
}

void CInitScript::InitConfig(std::map<std::string, std::vector<std::string>>& outProfiles)
{
	if (!script->Load("init", "init.as")) {
		return;
	}
	asIScriptModule* mod = script->GetEngine()->GetModule("init");
	int r = mod->SetDefaultNamespace("Init"); ASSERT(r >= 0);
	asIScriptFunction* init = script->GetFunc(mod, "void Init(dictionary@)");
	if (init == nullptr) {
		return;
	}

	CScriptDictionary* dict = CScriptDictionary::Create(script->GetEngine());

	asIScriptContext* ctx = script->PrepareContext(init);
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

		catDict->Release();
	}

	CScriptDictionary* profDict;
	if (dict->Get("profile", &profDict, dict->GetTypeId("profile"))) {
		CScriptArray* keys = profDict->GetKeys();
		for (unsigned i = 0; i < keys->GetSize(); ++i) {
			std::string* key = static_cast<std::string*>(keys->At(i));
			std::vector<std::string>& profile = outProfiles[*key];
			CScriptArray* value;
			if (profDict->Get(*key, &value, profDict->GetTypeId(*key))) {
				for (unsigned j = 0; j < value->GetSize(); ++j) {
					profile.push_back(*(std::string*)value->At(j));
				}

				value->Release();
			}
		}
		keys->Release();

		profDict->Release();
	}

	dict->Release();

	mod->Discard();
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
