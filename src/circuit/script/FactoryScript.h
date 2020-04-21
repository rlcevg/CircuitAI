/*
 * FactoryScript.h
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_

#include "script/Script.h"

class asIScriptFunction;

namespace circuit {

class CFactoryManager;
class IUnitTask;
class CCircuitUnit;

class CFactoryScript: public IScript {
public:
	CFactoryScript(CScriptManager* scr, CFactoryManager* mgr);
	virtual ~CFactoryScript();

	void Init() override;

public:
	IUnitTask* MakeTask(CCircuitUnit* unit);

private:
	struct SScriptInfo {
		asIScriptFunction* makeTask = nullptr;
	} info;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_
