/*
 * FactoryScript.h
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_

#include "script/UnitModuleScript.h"

namespace springai {
	class AIFloat3;
}

namespace circuit {

class CFactoryManager;
class CCircuitDef;

class CFactoryScript: public IUnitModuleScript {
public:
	CFactoryScript(CScriptManager* scr, CFactoryManager* mgr);
	virtual ~CFactoryScript();

	bool Init() override;

public:
	void FactoryCreated(CCircuitUnit* unit);
	void FactoryDestroyed(CCircuitUnit* unit);
	bool IsSwitchTime(int lastSwitchFrame);
	bool IsSwitchAllowed(CCircuitDef* facDef);

private:
	struct SScriptInfo {
		asIScriptFunction* factoryCreated = nullptr;
		asIScriptFunction* factoryDestroyed = nullptr;
		asIScriptFunction* isSwitchTime = nullptr;
		asIScriptFunction* isSwitchAllowed = nullptr;
	} factoryInfo;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_
