/*
 * UnitModuleScript.h
 *
 *  Created on: Jan 2, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_UNITMODULESCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_UNITMODULESCRIPT_H_

#include "script/ModuleScript.h"

class asIScriptModule;
class asIScriptFunction;

namespace circuit {

class IUnitModule;
class IUnitTask;
class CCircuitUnit;

class IUnitModuleScript: public IModuleScript {
public:
	IUnitModuleScript(CScriptManager* scr, IUnitModule* mod);
	virtual ~IUnitModuleScript();

protected:
	void InitModule(asIScriptModule* mod);

public:
	IUnitTask* MakeTask(CCircuitUnit* unit);
	void TaskCreated(IUnitTask* task);
	void TaskClosed(IUnitTask* task, bool done);

protected:
	struct SScriptInfo {
		asIScriptFunction* makeTask = nullptr;
		asIScriptFunction* taskCreated = nullptr;
		asIScriptFunction* taskClosed = nullptr;
	} info;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_UNITMODULESCRIPT_H_
