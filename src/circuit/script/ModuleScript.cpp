/*
 * ModuleScript.cpp
 *
 *  Created on: May 13, 2020
 *      Author: rlcevg
 */

#include "script/ModuleScript.h"

namespace circuit {

IModuleScript::IModuleScript(CScriptManager* scr, IModule* mod)
		: IScript(scr)
		, manager(mod)
{
}

IModuleScript::~IModuleScript()
{
}

} // namespace circuit
