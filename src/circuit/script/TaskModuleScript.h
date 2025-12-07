/*
 * TaskModuleScript.h
 *
 *  Created on: Jan 2, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_TASKMODULESCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_TASKMODULESCRIPT_H_

#include "script/ModuleScript.h"

class asIScriptModule;
class asIScriptFunction;

namespace circuit {

class ITaskModule;
class IUnitTask;
class CCircuitUnit;

class ITaskModuleScript: public IModuleScript {
public:
	ITaskModuleScript(CScriptManager* scr, ITaskModule* mod);
	virtual ~ITaskModuleScript();

protected:
	void InitModule(asIScriptModule* mod);

public:
	IUnitTask* MakeTask(CCircuitUnit* unit);
	void TaskAdded(IUnitTask* task);
	void TaskRemoved(IUnitTask* task, bool done);

protected:
	struct SScriptInfo {
		asIScriptFunction* makeTask = nullptr;
		asIScriptFunction* taskAdded = nullptr;
		asIScriptFunction* taskRemoved = nullptr;
	} umInfo;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_TASKMODULESCRIPT_H_
