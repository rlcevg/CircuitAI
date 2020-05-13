/*
 * Script.cpp
 *
 *  Created on: Apr 19, 2019
 *      Author: rlcevg
 */

#include "script/Script.h"

namespace circuit {

IScript::IScript(CScriptManager* scr)
		: script(scr)
{
}

IScript::~IScript()
{
}

} // namespace circuit
