/*
 * ScriptManager.h
 *
 *  Created on: Apr 3, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SETUP_SCRIPTMANAGER_H_
#define SRC_CIRCUIT_SETUP_SCRIPTMANAGER_H_

#include <vector>
#include <string>

class asIScriptEngine;
class asIScriptModule;
class asIScriptFunction;
class asIScriptContext;
class asSMessageInfo;
class asCJITCompiler;
class CScriptArray;

namespace circuit {

class CCircuitAI;

class CScriptManager {
public:
	CScriptManager(CCircuitAI* circuit);
	virtual ~CScriptManager();

private:
	void Init();
	void Release();
public:
	bool Load(const char* modname, const char* filename);
	asIScriptEngine* GetEngine() const { return engine; }
	asIScriptFunction* GetFunc(asIScriptModule* mod, const char* decl);
	asIScriptContext* PrepareContext(asIScriptFunction* func);
	void ReturnContext(asIScriptContext* ctx);
	bool Exec(asIScriptContext* ctx);

private:
	CCircuitAI* circuit;
	asIScriptEngine* engine;
	// Our pool of script contexts. This is used to avoid allocating
	// the context objects all the time. The context objects are quite
	// heavy weight and should be shared between function calls.
	std::vector<asIScriptContext*> contexts;
	asCJITCompiler* jit;

	static asIScriptContext* ProvideContext(asIScriptEngine*, void*);
	static void StoreContext(asIScriptEngine*, asIScriptContext*, void*);

	void MessageCallback(const asSMessageInfo *msg, void *param);

	void Log(std::string &msg);
	int Dice(const CScriptArray* array);
	template<typename T> T Max(T l, T r) { return std::max(l, r); }

	bool LocatePath(std::string& filename);

	void RegisterUtils();
	void RegisterSpringai();
	void RegisterCircuitAI();
public:
	void RegisterMgr();
};

} // namespace circuit

#endif // SRC_CIRCUIT_SETUP_SCRIPTMANAGER_H_
