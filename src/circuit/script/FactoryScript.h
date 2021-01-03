/*
 * FactoryScript.h
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_

#include "script/UnitModuleScript.h"

namespace circuit {

class CFactoryManager;

class CFactoryScript: public IUnitModuleScript {
public:
	CFactoryScript(CScriptManager* scr, CFactoryManager* mgr);
	virtual ~CFactoryScript();

	void Init() override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_FACTORYSCRIPT_H_
