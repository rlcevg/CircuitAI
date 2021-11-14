/*
 * ScriptManager.cpp
 *
 *  Created on: Apr 3, 2019
 *      Author: rlcevg
 */

#include "script/ScriptManager.h"
#ifdef DEBUG_VIS
#include "script/Script.h"
#endif
#include "CircuitAI.h"
#include "util/FileSystem.h"
#include "util/Utils.h"

#include "Log.h"

//#define AS_USE_STLNAMES		1

#include "angelscript/include/angelscript.h"
#include "angelscript/jit/as_jit.h"
#include "angelscript/add_on/scriptstdstring/scriptstdstring.h"
#include "angelscript/add_on/scriptarray/scriptarray.h"
#include "angelscript/add_on/scriptdictionary/scriptdictionary.h"
#include "angelscript/add_on/scriptmath/scriptmath.h"
#include "angelscript/add_on/scriptbuilder/scriptbuilder.h"
#include "angelscript/add_on/aatc/aatc.hpp"

namespace circuit {

using namespace springai;

std::string CScriptManager::mainName("main");

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

#ifdef CIRCUIT_AS_JIT
	// Create the JIT Compiler. The build flags are explained below,
	// as well as in as_jit.h
	//  Faster: JIT_NO_SUSPEND | JIT_SYSCALL_FPU_NORESET | JIT_SYSCALL_NO_ERRORS | JIT_ALLOC_SIMPLE | JIT_FAST_REFCOUNT
	//  Slower: JIT_NO_SWITCHES | JIT_NO_SCRIPT_CALLS
	jit = new asCJITCompiler(JIT_FAST_REFCOUNT | JIT_SYSCALL_FPU_NORESET);
	// Enable JIT helper instructions; without these,
	// the JIT will not be invoked
	r = engine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, true); ASSERT(r >= 0);
	// Bind the JIT compiler to the engine
	r = engine->SetJITCompiler(jit); ASSERT(r >= 0);
#endif

	// AngelScript doesn't have a built-in string type, as there is no definite standard
	// string type for C++ applications. Every developer is free to register its own string type.
	// The SDK do however provide a standard add-on for registering a string type, so it's not
	// necessary to implement the registration yourself if you don't want to.
	RegisterStdString(engine);
	RegisterScriptArray(engine, true);
//	RegisterStdStringUtils(engine);  // optional
	RegisterScriptDictionary(engine);
	RegisterScriptMath(engine);
	aatc::RegisterAllContainers(engine);

	engine->SetContextCallbacks(CScriptManager::ProvideContext, CScriptManager::StoreContext, this);
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

bool CScriptManager::Load(const char* modname, const std::string& subdir, const std::string& filename)
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

	std::string dirname = utils::GetAIDataGameDir(circuit->GetSkirmishAI(), "script") + subdir + SLASH;
	if (utils::FileExists(circuit->GetCallback(), dirname + filename)) {  // Locate game-side script
		builder.SetReadFunc([this](const std::string& filename) {
			return utils::ReadFile(circuit->GetCallback(), filename);
		});
	} else {
		circuit->LOG("Game-side script: '%s' is missing!", (dirname + filename).c_str());
		dirname = "script" SLASH + subdir + SLASH;
		if (!utils::LocatePath(circuit->GetCallback(), dirname)) {  // Locate AI script
			circuit->LOG("AI script: '%s' is missing!", (dirname + filename).c_str());
			return false;
		}
	}

	circuit->LOG("Load script: %s", (dirname + filename).c_str());
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
	SCOPED_TIME(circuit, std::string(ctx->GetFunction()->GetNamespace()) + "::" + ctx->GetFunction()->GetName());

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

#ifdef DEBUG_VIS
void CScriptManager::Reload()
{
	int r = engine->DiscardModule(mainName.c_str()); ASSERT(r >= 0);
	for (IScript* scr : scripts) {
		scr->Init();
	}
}
#endif

} // namespace circuit
