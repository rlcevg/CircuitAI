/*
 * EnemyUnit.cpp
 *
 *  Created on: Aug 30, 2015
 *      Author: rlcevg
 */

#include "unit/EnemyUnit.h"
#include "unit/CircuitDef.h"
#include "task/fighter/FighterTask.h"
#include "util/utils.h"

#include "Weapon.h"

namespace circuit {

using namespace springai;

CEnemyUnit::CEnemyUnit(Unit* unit, CCircuitDef* cdef)
		: id(unit->GetUnitId())
		, unit(unit)
		, lastSeen(-1)
		, dgun(nullptr)
		, pos(ZeroVector)
		, threat(.0f)
		, range({0})
		, losStatus(LosMask::NONE)
{
	SetCircuitDef(cdef);
}

CEnemyUnit::~CEnemyUnit()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (IFighterTask* task : tasks) {
		task->ClearTarget();
	}

	delete unit;
	delete dgun;
}

void CEnemyUnit::SetCircuitDef(CCircuitDef* cdef)
{
	circuitDef = cdef;
	delete dgun;
	if (cdef == nullptr) {
		dgun = nullptr;
	} else {
		WeaponMount* wpMnt = circuitDef->GetDGunMount();
		dgun = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
	}
}

bool CEnemyUnit::IsDisarmed()
{
	return unit->GetRulesParamFloat("disarmed", 0) > .0f;
}

float CEnemyUnit::GetDPS()
{
	if (circuitDef == nullptr) {  // unknown enemy is a threat
		return 16.0f;
	}
	float dps = circuitDef->GetDPS();
	if (dps < 0.1f) {
		return .0f;
	}
	if (unit->IsParalyzed() || unit->IsBeingBuilt() || IsDisarmed()) {
		return 1.0f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return dps;
}

} // namespace circuit
