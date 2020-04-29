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

namespace springai {
	class AIFloat3;
}

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

	void Log(const std::string &msg) const;
	void AddPoint(const springai::AIFloat3& pos, const std::string &msg) const;
	void DelPoint(const springai::AIFloat3& pos) const;
	int Dice(const CScriptArray* array) const;
	template<typename T> T Max(T l, T r) const { return std::max(l, r); }

	bool LocatePath(std::string& filename);

	void RegisterSpringai();
	void RegisterUtils();
	void RegisterCircuitAI();
public:
	void RegisterMgr();
};

} // namespace circuit

#endif // SRC_CIRCUIT_SETUP_SCRIPTMANAGER_H_
