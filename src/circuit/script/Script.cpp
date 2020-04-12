/*
 * Script.cpp
 *
 *  Created on: Apr 19, 2019
 *      Author: rlcevg
 */

#include "script/Script.h"

namespace circuit {

IScript::IScript(CScriptManager* scr, IModule* mod)
		: script(scr)
		, manager(mod)
{
}

IScript::~IScript()
{
}

} // namespace circuit
