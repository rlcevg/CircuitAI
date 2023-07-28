/*
 * CircuitWDef.cpp
 *
 *  Created on: Oct 27, 2019
 *      Author: rlcevg
 */

#include "unit/CircuitWDef.h"
#include "util/Defines.h"

#include "Lua/LuaConfig.h"

namespace circuit {

using namespace springai;

CWeaponDef::CWeaponDef(WeaponDef* def, Resource* resE)
		: def(def)
{
	range = def->GetRange();
	aoe = def->GetAreaOfEffect();
	costE = def->GetCost(resE);

	std::string wt(def->GetType());
	fireTime = (wt == "BeamLaser") ? def->GetBeamTime() : 0.f;

	isStockpile = def->IsStockpileable();
	if (isStockpile) {
		const float stockTime = def->GetStockpileTime() / FRAMES_PER_SEC;
		costE /= stockTime;
	}

	isHigh = (wt == "StarburstLauncher")
			|| ((wt == "MissileLauncher") && (def->GetTrajectoryHeight() > 0.5f))  // 1.0 ~ 45 deg
			|| ((wt == "Cannon") && (def->GetHighTrajectory() >= 1));
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
