/*
 * Script.h
 *
 *  Created on: Apr 19, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_SCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_SCRIPT_H_

namespace circuit {

class CScriptManager;
class IModule;

class IScript {
public:
	IScript(CScriptManager* scr, IModule* mod);
	virtual ~IScript();

	virtual void Init() = 0;

protected:
	CScriptManager* script;
	IModule* manager;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_SCRIPT_H_
