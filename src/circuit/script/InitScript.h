/*
 * InitScript.h
 *
 *  Created on: May 13, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_INITSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_INITSCRIPT_H_

#include "script/Script.h"

#include <string>

namespace springai {
	class AIFloat3;
}

class asIScriptFunction;
class CScriptArray;

namespace circuit {

class CCircuitAI;

class CInitScript: public IScript {
public:
	CInitScript(CScriptManager* scr, CCircuitAI* circuit);
	virtual ~CInitScript();

	void Init() override;

	void RegisterMgr();

private:
	void InitConfig();

	void Log(const std::string& msg) const;
	void AddPoint(const springai::AIFloat3& pos, const std::string& msg) const;
	void DelPoint(const springai::AIFloat3& pos) const;
	int Dice(const CScriptArray* array) const;
	template<typename T> T Max(T l, T r) const { return std::max(l, r); }

	CCircuitAI* circuit;
	struct SScriptInfo {
		asIScriptFunction* init = nullptr;
	} info;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_INITSCRIPT_H_
