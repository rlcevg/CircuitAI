/*
 * UnitModuleScript.h
 *
 *  Created on: Jan 2, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_UNITMODULESCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_UNITMODULESCRIPT_H_

#include "script/ModuleScript.h"
#include "module/UnitModule.h"

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
	void TaskAdded(IUnitTask* task);
	void TaskRemoved(IUnitTask* task, bool done);
	void UnitAdded(CCircuitUnit* unit, IUnitModule::UseAs usage);
	void UnitRemoved(CCircuitUnit* unit, IUnitModule::UseAs usage);

protected:
	struct SScriptInfo {
		asIScriptFunction* makeTask = nullptr;
		asIScriptFunction* taskAdded = nullptr;
		asIScriptFunction* taskRemoved = nullptr;
		asIScriptFunction* unitAdded = nullptr;
		asIScriptFunction* unitRemoved = nullptr;
	} umInfo;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_UNITMODULESCRIPT_H_
