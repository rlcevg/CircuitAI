/*
 * BuilderScript.h
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_BUILDERSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_BUILDERSCRIPT_H_

#include "script/UnitModuleScript.h"

namespace circuit {

class CBuilderManager;

class CBuilderScript: public IUnitModuleScript {
public:
	CBuilderScript(CScriptManager* scr, CBuilderManager* mgr);
	virtual ~CBuilderScript();

	void Init() override;

public:
	void WorkerCreated(CCircuitUnit* unit);
	void WorkerDestroyed(CCircuitUnit* unit);

private:
	struct SScriptInfo {
		asIScriptFunction* workerCreated = nullptr;
		asIScriptFunction* workerDestroyed = nullptr;
	} info;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_BUILDERSCRIPT_H_
