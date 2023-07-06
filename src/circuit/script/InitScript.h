/*
 * InitScript.h
 *
 *  Created on: May 13, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_INITSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_INITSCRIPT_H_

#include "util/MaskHandler.h"

#include <string>
#include <map>

class asIScriptFunction;
class CScriptArray;

namespace circuit {

class CScriptManager;
class CCircuitAI;

class CInitScript {
public:
	CInitScript(CScriptManager* scr, CCircuitAI* circuit);
	virtual ~CInitScript();

	void InitConfig(std::map<std::string, std::vector<std::string>>& outProfiles);

private:
	CMaskHandler::TypeMask AddRole(const std::string& name, int actAsRole);

	void Log(const std::string& msg) const;
	int Dice(const CScriptArray* array) const;
	template<typename T> T Max(T l, T r) const { return std::max(l, r); }

	CScriptManager* script;
	CCircuitAI* circuit;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_INITSCRIPT_H_
