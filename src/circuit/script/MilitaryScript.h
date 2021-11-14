/*
 * MilitaryScript.h
 *
 *  Created on: Apr 4, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_MILITARYSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_MILITARYSCRIPT_H_

#include "script/UnitModuleScript.h"

namespace springai {
	class AIFloat3;
}

namespace circuit {

class CMilitaryManager;

class CMilitaryScript: public IUnitModuleScript {
public:
	CMilitaryScript(CScriptManager* scr, CMilitaryManager* mgr);
	virtual ~CMilitaryScript();

	bool Init() override;

public:
	void MakeDefence(int cluster, const springai::AIFloat3& pos);
	bool IsAirValid();

private:
	struct SScriptInfo {
		asIScriptFunction* makeDefence = nullptr;
		asIScriptFunction* isAirValid = nullptr;
	} militaryInfo;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_MILITARYSCRIPT_H_
