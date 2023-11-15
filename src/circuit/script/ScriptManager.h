/*
 * ScriptManager.h
 *
 *  Created on: Apr 3, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SETUP_SCRIPTMANAGER_H_
#define SRC_CIRCUIT_SETUP_SCRIPTMANAGER_H_

#include "System/Threading/SpringThreading.h"

#include <vector>
#include <string>

class asIScriptEngine;
class asIScriptModule;
class asIScriptFunction;
class asIScriptContext;
class asSMessageInfo;
class asCJITCompiler;

namespace circuit {

class CCircuitAI;
#ifdef DEBUG_VIS
class IScript;
#endif

class CScriptManager {
public:
	static std::string initName;
	static std::string mainName;

	CScriptManager(CCircuitAI* circuit);
	virtual ~CScriptManager();

private:
	void Init();
	void Release();
public:
	bool Load(const char* modname, const std::string& subdir, const std::string& filename);
	asIScriptEngine* GetEngine() const { return engine; }
	asIScriptFunction* GetFunc(asIScriptModule* mod, const char* decl);
	asIScriptContext* RequestContext();  // thread
	asIScriptContext* PrepareContext(asIScriptFunction* func);
	void ReturnContext(asIScriptContext* ctx);
	void ReleaseContext(asIScriptContext* ctx);
	bool Exec(asIScriptContext* ctx);

private:
	CCircuitAI* circuit;
	asIScriptEngine* engine;
	// Our pool of script contexts. This is used to avoid allocating
	// the context objects all the time. The context objects are quite
	// heavy weight and should be shared between function calls.
	std::vector<asIScriptContext*> contexts;
	asCJITCompiler* jit;
	spring::mutex mtx;

	static asIScriptContext* ProvideContext(asIScriptEngine*, void*);
	static void StoreContext(asIScriptEngine*, asIScriptContext*, void*);

	void MessageCallback(const asSMessageInfo *msg, void *param);

#ifdef DEBUG_VIS
public:
	void AddScript(IScript* scr) { scripts.push_back(scr); }
	void Reload();

private:
	std::vector<IScript*> scripts;
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_SETUP_SCRIPTMANAGER_H_
