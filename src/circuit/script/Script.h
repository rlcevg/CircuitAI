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

class IScript {
public:
	IScript(CScriptManager* scr);
	virtual ~IScript();

	virtual void Init() = 0;

protected:
	CScriptManager* script;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_SCRIPT_H_
