/*
 * BuilderScript.h
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_BUILDERSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_BUILDERSCRIPT_H_

#include "script/ModuleScript.h"

class asIScriptFunction;

namespace circuit {

class CBuilderManager;
class IUnitTask;
class CCircuitUnit;

class CBuilderScript: public IModuleScript {
public:
	CBuilderScript(CScriptManager* scr, CBuilderManager* mgr);
	virtual ~CBuilderScript();

	void Init() override;

public:
	IUnitTask* MakeTask(CCircuitUnit* unit);

private:
	struct SScriptInfo {
		asIScriptFunction* makeTask = nullptr;
	} info;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_BUILDERSCRIPT_H_
