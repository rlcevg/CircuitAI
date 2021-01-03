/*
 * Script.cpp
 *
 *  Created on: Apr 19, 2019
 *      Author: rlcevg
 */

#include "script/Script.h"
#ifdef DEBUG_VIS
#include "script/ScriptManager.h"
#endif

namespace circuit {

IScript::IScript(CScriptManager* scr)
		: script(scr)
{
#ifdef DEBUG_VIS
	scr->AddScript(this);
#endif
}

IScript::~IScript()
{
}

} // namespace circuit
