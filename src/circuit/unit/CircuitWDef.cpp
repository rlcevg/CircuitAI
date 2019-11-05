/*
 * CircuitWDef.cpp
 *
 *  Created on: Oct 27, 2019
 *      Author: rlcevg
 */

#include "unit/CircuitWDef.h"

#include "Lua/LuaConfig.h"

namespace circuit {

using namespace springai;

CWeaponDef::CWeaponDef(WeaponDef* def)
		: def(def)
{
	range = def->GetRange();
	aoe = def->GetAreaOfEffect();
}

CWeaponDef::~CWeaponDef()
{
	delete def;
}

CWeaponDef::Id CWeaponDef::WeaponIdFromLua(int luaId)
{
	return luaId - LUA_WEAPON_BASE_INDEX;
}

} // namespace circuit
