/*
 * ModuleScript.h
 *
 *  Created on: May 13, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_MODULESCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_MODULESCRIPT_H_

#include "script/Script.h"
#include "module/Module.h"

#include <iostream>

class asIScriptModule;
class asIScriptFunction;

namespace circuit {

class CCircuitUnit;

class IModuleScript: public IScript {
public:
	IModuleScript(CScriptManager* scr, IModule* mod);
	virtual ~IModuleScript();

protected:
	void InitModule(asIScriptModule* mod);

public:
	void UnitAdded(CCircuitUnit* unit, IModule::UseAs usage);
	void UnitRemoved(CCircuitUnit* unit, IModule::UseAs usage);
	void Load(std::istream& is);
	void Save(std::ostream& os) const;

protected:
	IModule* manager;
	struct SScriptInfo {
		asIScriptFunction* unitAdded = nullptr;
		asIScriptFunction* unitRemoved = nullptr;
		asIScriptFunction* load = nullptr;
		asIScriptFunction* save = nullptr;
	} mInfo;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_MODULESCRIPT_H_
