/*
 * ScriptManager.cpp
 *
 *  Created on: Apr 3, 2019
 *      Author: rlcevg
 */

#include "script/ScriptManager.h"
#include "script/RefCounter.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/MaskHandler.h"
#include "util/FileSystem.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"

#include "DataDirs.h"
#include "Log.h"

#include "angelscript/include/angelscript.h"
#include "angelscript/jit/as_jit.h"
#include "angelscript/add_on/scriptstdstring/scriptstdstring.h"
#include "angelscript/add_on/scriptarray/scriptarray.h"
#include "angelscript/add_on/scriptdictionary/scriptdictionary.h"
#include "angelscript/add_on/scriptbuilder/scriptbuilder.h"
#include "angelscript/add_on/aatc/aatc.hpp"

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

CScriptManager::CScriptManager(CCircuitAI* circuit)
		: circuit(circuit)
		, engine(nullptr)
		, jit(nullptr)
{
	Init();
}

CScriptManager::~CScriptManager()
{
	Release();
}

void CScriptManager::Init()
{
	// Create the script engine
	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	// Set the message callback to receive information on errors in human readable form.
	int r = engine->SetMessageCallback(asMETHOD(CScriptManager, MessageCallback), this, asCALL_THISCALL); ASSERT(r >= 0);

	r = engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES,            false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE,                   true); ASSERT(r >= 0);  // Default: true
	r = engine->SetEngineProperty(asEP_COPY_SCRIPT_SECTIONS,                true); ASSERT(r >= 0);  // Default: true
	r = engine->SetEngineProperty(asEP_MAX_STACK_SIZE,                         0); ASSERT(r >= 0);  // Default: 0 (no limit)
	r = engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS,             false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_ALLOW_MULTILINE_STRINGS,             true); ASSERT(r >= 0);  //** Default: false
	r = engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES,        false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES,            false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD,        true); ASSERT(r >= 0);  // Default: true
	r = engine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE,                 false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_SCRIPT_SCANNER,                         1); ASSERT(r >= 0);  // Default: 1 (UTF8)  // 0 - ASCII, 1 - UTF8
//	r = engine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS,           false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_STRING_ENCODING,                        0); ASSERT(r >= 0);  // Default: 0 (UTF8)  // 0 - UTF8/ASCII, 1 - UTF16
	r = engine->SetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE,                 3); ASSERT(r >= 0);  // Default: 3  // 0 - no accessors, 1 - app registered accessors, 2 - app and script created accessors, 3 - app and script created accesors, property keyword required
	r = engine->SetEngineProperty(asEP_EXPAND_DEF_ARRAY_TO_TMPL,           false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT,                true); ASSERT(r >= 0);  // Default: true
	r = engine->SetEngineProperty(asEP_DISALLOW_GLOBAL_VARS,               false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT,      false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_COMPILER_WARNINGS,                      2); ASSERT(r >= 0);  //** 0 - dismiss, 1 - emit, 2 - treat as error
	r = engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_ALTER_SYNTAX_NAMED_ARGS,                0); ASSERT(r >= 0);  // Default: 0  // 0 - no change, 1 - accept = but warn, 2 - accept = without warning
	r = engine->SetEngineProperty(asEP_DISABLE_INTEGER_DIVISION,           false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_DISALLOW_EMPTY_LIST_ELEMENTS,       false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_PRIVATE_PROP_AS_PROTECTED,          false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_ALLOW_UNICODE_IDENTIFIERS,          false); ASSERT(r >= 0);  // Default: false
	r = engine->SetEngineProperty(asEP_HEREDOC_TRIM_MODE,                      1); ASSERT(r >= 0);  // Default: 1  // 0 - never trim, 1 - trim if multiple lines, 2 - always trim
	r = engine->SetEngineProperty(asEP_MAX_NESTED_CALLS,                     100); ASSERT(r >= 0);  // Default: 100
	r = engine->SetEngineProperty(asEP_GENERIC_CALL_MODE,                      1); ASSERT(r >= 0);  // Default: 1  // 0 - ignore auto handles, 1 - treat them the same way as native calling convention
	r = engine->SetEngineProperty(asEP_INIT_STACK_SIZE,                     4096); ASSERT(r >= 0);  // Default: 4096
	r = engine->SetEngineProperty(asEP_INIT_CALL_STACK_SIZE,                  10); ASSERT(r >= 0);  // Default: 10
	r = engine->SetEngineProperty(asEP_MAX_CALL_STACK_SIZE,                    0); ASSERT(r >= 0);  // Default: 0 (no limit)

	// Create the JIT Compiler. The build flags are explained below,
	// as well as in as_jit.h
	//  Faster: JIT_NO_SUSPEND | JIT_SYSCALL_FPU_NORESET | JIT_SYSCALL_NO_ERRORS | JIT_ALLOC_SIMPLE | JIT_FAST_REFCOUNT
	//  Slower: JIT_NO_SWITCHES | JIT_NO_SCRIPT_CALLS
	jit = new asCJITCompiler(JIT_FAST_REFCOUNT | JIT_SYSCALL_FPU_NORESET);
	// Enable JIT helper instructions; without these,
	// the JIT will not be invoked
//	r = engine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, true); ASSERT(r >= 0);
//	// Bind the JIT compiler to the engine
//	r = engine->SetJITCompiler(jit); ASSERT(r >= 0);

	// AngelScript doesn't have a built-in string type, as there is no definite standard
	// string type for C++ applications. Every developer is free to register its own string type.
	// The SDK do however provide a standard add-on for registering a string type, so it's not
	// necessary to implement the registration yourself if you don't want to.
	RegisterStdString(engine);
	RegisterScriptArray(engine, true);
	RegisterScriptDictionary(engine);
	aatc::RegisterAllContainers(engine);

	engine->SetContextCallbacks(CScriptManager::ProvideContext, CScriptManager::StoreContext, this);

	RegisterUtils();
	RegisterSpringai();
	RegisterCircuitAI();
}

void CScriptManager::Release()
{
	// Clean up
	for (asIScriptContext* context : contexts) {
		context->Release();
	}
	contexts.clear();
	engine->ShutDownAndRelease();
	delete jit;
}

bool CScriptManager::Load(const char* modname, const char* filename)
{
	// The CScriptBuilder helper is an add-on that loads the file,
	// performs a pre-processing pass if necessary, and then tells
	// the engine to build a script module.
	CScriptBuilder builder;
	int r = builder.StartNewModule(engine, modname);
	if (r < 0) {
		// If the code fails here it is usually because there
		// is no more memory to allocate the module
		circuit->LOG("Script: Unrecoverable error while starting a new module!");
		return false;
	}

	std::string dirname = "script" SLASH;
	if (!LocatePath(dirname)) {
		return false;
	}

	r = builder.AddSectionFromFile((dirname + filename).c_str());
	if (r < 0) {
		// The builder wasn't able to load the string. Maybe some
		// preprocessing commands are incorrectly written.
		circuit->LOG("Script: Unable to add section!");
		return false;
	}
	r = builder.BuildModule();
	if (r < 0) {
		// An error occurred. Instruct the script writer to fix the
		// compilation errors that were listed in the output stream.
		circuit->LOG("Script: Fix compilation errors!");
		return false;
	}
	return true;
}

asIScriptFunction* CScriptManager::GetFunc(asIScriptModule* mod, const char* decl)
{
	// Find the function that is to be called.
	asIScriptFunction* func = mod->GetFunctionByDecl(decl);
	if (func == nullptr) {
		// The function couldn't be found. Instruct the script writer
		// to include the expected function in the script.
		circuit->LOG("Script: '%s' not found!", decl);
	}
	return func;
}

asIScriptContext* CScriptManager::PrepareContext(asIScriptFunction* func)
{
	asIScriptContext* ctx = engine->RequestContext();
	int r = ctx->Prepare(func); ASSERT(r >= 0);
	return ctx;
}

void CScriptManager::ReturnContext(asIScriptContext* ctx)
{
//	ctx->Unprepare();
	engine->ReturnContext(ctx);
}

bool CScriptManager::Exec(asIScriptContext* ctx)
{
	SCOPED_TIME(circuit, ctx->GetFunction()->GetName());

	int r = ctx->Execute();
	if (r != asEXECUTION_FINISHED) {
		// The execution didn't complete as expected. Determine what happened.
		if (r == asEXECUTION_EXCEPTION) {
			// An exception occurred, let the script writer know what happened so it can be corrected.
			circuit->LOG("Script"
						 "\n  Exception: %s"
						 "\n  Function: %s"
						 "\n  Line: %i",
						 ctx->GetExceptionString(),
						 ctx->GetExceptionFunction()->GetDeclaration(),
						 ctx->GetExceptionLineNumber());
		}
		return false;
	}
	return true;
}

asIScriptContext* CScriptManager::ProvideContext(asIScriptEngine* engine, void* param)
{
	CScriptManager* mgr = static_cast<CScriptManager*>(param);

	asIScriptContext* ctx;
	if (!mgr->contexts.empty()) {
		ctx = *mgr->contexts.rbegin();
		mgr->contexts.pop_back();
	} else {
		ctx = engine->CreateContext();
	}

	return ctx;
}

void CScriptManager::StoreContext(asIScriptEngine* engine, asIScriptContext* ctx, void* param)
{
	static_cast<CScriptManager*>(param)->contexts.push_back(ctx);
}

// Implement a simple message callback function
void CScriptManager::MessageCallback(const asSMessageInfo* msg, void* param)
{
	const char* type = "ERR ";
	if (msg->type == asMSGTYPE_WARNING) {
		type = "WARN";
	} else if (msg->type == asMSGTYPE_INFORMATION) {
		type = "INFO";
	}
	circuit->LOG("%s (%d, %d) : %s : %s", msg->section, msg->row, msg->col, type, msg->message);
}

void CScriptManager::Log(std::string &msg)
{
	circuit->LOG("%s", msg.c_str());
}

int CScriptManager::Dice(const CScriptArray* array)
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

bool CScriptManager::LocatePath(std::string& dirname)
{
	DataDirs* datadirs = circuit->GetCallback()->GetDataDirs();
	const bool located = utils::LocatePath(datadirs, dirname);
	delete datadirs;
	if (!located) {
		circuit->LOG("Script: '%s' is missing!", dirname.c_str());
	}
	return located;
}

void CScriptManager::RegisterUtils()
{
	int r = engine->RegisterGlobalFunction("void aiLog(const string& in)", asMETHOD(CScriptManager, Log), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int aiDice(const array<float>@+)", asMETHOD(CScriptManager, Dice), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int aiMax(int, int)", asMETHODPR(CScriptManager, Max<int>, (int, int), int), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("float aiMax(float, float)", asMETHODPR(CScriptManager, Max<float>, (float, float), float), asCALL_THISCALL_ASGLOBAL, this); ASSERT(r >= 0);
}

void CScriptManager::RegisterSpringai()
{
	int r = engine->RegisterObjectType("AIFloat3", sizeof(AIFloat3), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<AIFloat3>()); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("AIFloat3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructAIFloat3), asCALL_CDECL_OBJLAST); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("AIFloat3", asBEHAVE_CONSTRUCT, "void f(const AIFloat3& in)", asFUNCTION(ConstructCopyAIFloat3), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("AIFloat3", "float x", asOFFSET(AIFloat3, x)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("AIFloat3", "float y", asOFFSET(AIFloat3, y)); ASSERT(r >= 0);
	r = engine->RegisterObjectProperty("AIFloat3", "float z", asOFFSET(AIFloat3, z)); ASSERT(r >= 0);
}

void CScriptManager::RegisterCircuitAI()
{
	int r = engine->RegisterObjectType("CCircuitAI", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CCircuitAI ai", circuit); ASSERT(r >= 0);

	r = engine->RegisterObjectType("CCircuitDef", 0, asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectType("CCircuitUnit", 0, asOBJ_REF | asOBJ_NOCOUNT); ASSERT(r >= 0);
	r = engine->RegisterObjectType("IUnitTask", 0, asOBJ_REF); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("IUnitTask", asBEHAVE_ADDREF, "void f()", asMETHODPR(IRefCounter, AddRef, (), int), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectBehaviour("IUnitTask", asBEHAVE_RELEASE, "void f()", asMETHODPR(IRefCounter, Release, (), int), asCALL_THISCALL); ASSERT(r >= 0);

	r = engine->RegisterObjectMethod("CCircuitAI", "int GetLastFrame() const", asMETHOD(CCircuitAI, GetLastFrame), asCALL_THISCALL); ASSERT(r >= 0);
//	r = engine->RegisterObjectMethod("CCircuitAI", "CCircuitDef@ GetCircuitDef(const string& in)", asMETHODPR(CCircuitAI, GetCircuitDef, (const std::string&), CCircuitDef*), asCALL_THISCALL); ASSERT(r >= 0);
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
	r = engine->RegisterGlobalProperty("CMaskHandler roleMasker", roleMasker); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CMaskHandler", "TypeMask GetTypeMask(const string& in)", asMETHOD(CMaskHandler, GetTypeMask), asCALL_THISCALL); ASSERT(r >= 0);

	r = engine->RegisterTypedef("Id", "int"); ASSERT(r >= 0);

	r = engine->RegisterObjectMethod("CCircuitDef", "bool IsRoleAny(Mask) const", asMETHOD(CCircuitDef, IsRoleAny), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "const string GetName() const", asFUNCTION(CCircuitDef_GetName), asCALL_CDECL_OBJFIRST); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "Id GetId() const", asMETHOD(CCircuitDef, GetId), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitDef", "bool IsAvailable(int)", asMETHODPR(CCircuitDef, IsAvailable, (int) const, bool), asCALL_THISCALL); ASSERT(r >= 0);

	r = engine->RegisterObjectMethod("CCircuitUnit", "Id GetId() const", asMETHODPR(CCircuitUnit, GetId, () const, ICoreUnit::Id), asCALL_THISCALL); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CCircuitUnit", "CCircuitDef@ GetCircuitDef() const", asMETHODPR(CCircuitUnit, GetCircuitDef, () const, CCircuitDef*), asCALL_THISCALL); ASSERT(r >= 0);

	r = engine->RegisterObjectMethod("IUnitTask", "int GetRefCount()", asMETHODPR(IRefCounter, GetRefCount, (), int), asCALL_THISCALL); ASSERT(r >= 0);

	Load("init", "init.as");
	asIScriptFunction* func = GetFunc(engine->GetModule("init"), "void init()");
	if (func != nullptr) {
		asIScriptContext* ctx = PrepareContext(func);
		Exec(ctx);
		ReturnContext(ctx);
	}
}

void CScriptManager::RegisterMgr()
{
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	int r = engine->RegisterObjectType("CTerrainManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CTerrainManager terrainMgr", terrainManager); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int GetTerrainWidth()", asFUNCTION(CTerrainManager::GetTerrainWidth), asCALL_CDECL); ASSERT(r >= 0);
	r = engine->RegisterGlobalFunction("int GetTerrainHeight()", asFUNCTION(CTerrainManager::GetTerrainHeight), asCALL_CDECL); ASSERT(r >= 0);

	CSetupManager* setupManager = circuit->GetSetupManager();
	r = engine->RegisterObjectType("CSetupManager", 0, asOBJ_REF | asOBJ_NOHANDLE); ASSERT(r >= 0);
	r = engine->RegisterGlobalProperty("CSetupManager setupMgr", setupManager); ASSERT(r >= 0);
	r = engine->RegisterObjectMethod("CSetupManager", "CCircuitDef@ GetCommChoice() const", asMETHOD(CSetupManager, GetCommChoice), asCALL_THISCALL); ASSERT(r >= 0);
}

} // namespace circuit
