/*
 * MilitaryScript.h
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_MILITARYSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_MILITARYSCRIPT_H_

#include "script/ModuleScript.h"

class asIScriptFunction;

namespace circuit {

class CMilitaryManager;
class IUnitTask;
class CCircuitUnit;

class CMilitaryScript: public IModuleScript {
public:
	CMilitaryScript(CScriptManager* scr, CMilitaryManager* mgr);
	virtual ~CMilitaryScript();

	void Init() override;

public:
	IUnitTask* MakeTask(CCircuitUnit* unit);
	bool IsAirValid();

private:
	struct SScriptInfo {
		asIScriptFunction* makeTask = nullptr;
		asIScriptFunction* isAirValid = nullptr;
	} info;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_MILITARYSCRIPT_H_
