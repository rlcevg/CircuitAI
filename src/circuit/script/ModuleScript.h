/*
 * ModuleScript.h
 *
 *  Created on: May 13, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_MODULESCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_MODULESCRIPT_H_

#include "script/Script.h"

namespace circuit {

class IModule;

class IModuleScript: public IScript {
public:
	IModuleScript(CScriptManager* scr, IModule* mod);
	virtual ~IModuleScript();

protected:
	IModule* manager;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_MODULESCRIPT_H_
