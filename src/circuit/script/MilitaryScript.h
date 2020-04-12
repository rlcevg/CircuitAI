/*
 * MilitaryScript.h
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_MILITARYSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_MILITARYSCRIPT_H_

#include "script/Script.h"

class asIScriptFunction;

namespace circuit {

class CMilitaryManager;

class CMilitaryScript: public IScript {
public:
	CMilitaryScript(CScriptManager* scr, CMilitaryManager* mgr);
	virtual ~CMilitaryScript();

	void Init() override;

public:
	bool IsAirValid();

private:
	struct SScriptInfo {
		SScriptInfo() : isAirValid(nullptr) {}
		asIScriptFunction* isAirValid;
	} info;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_MILITARYSCRIPT_H_
