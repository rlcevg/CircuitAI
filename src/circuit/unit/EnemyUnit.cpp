/*
 * EnemyUnit.cpp
 *
 *  Created on: Aug 30, 2015
 *      Author: rlcevg
 */

#include "unit/EnemyUnit.h"
#include "task/fighter/FighterTask.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CEnemyUnit::CEnemyUnit(Id unitId, Unit* unit, CCircuitDef* cdef)
		: ICoreUnit(unitId, unit, cdef)
		, lastSeen(-1)
		, pos(ZeroVector)
		, threat(.0f)
		, range({0})
		, losStatus(LosMask::NONE)
{
	cost = (cdef == nullptr) ? 0.f : cdef->GetCost();
}

CEnemyUnit::~CEnemyUnit()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (IFighterTask* task : tasks) {
		task->ClearTarget();
	}
}

void CEnemyUnit::SetCircuitDef(CCircuitDef* cdef)
{
	circuitDef = cdef;
	cost = (cdef == nullptr) ? 0.f : cdef->GetCost();
}

bool CEnemyUnit::IsDisarmed()
{
	return unit->GetRulesParamFloat("disarmed", 0) > .0f;
}

bool CEnemyUnit::IsAttacker()
{
	if (circuitDef == nullptr) {  // unknown enemy is a threat
		return true;
	}
	return circuitDef->IsAttacker();
}

float CEnemyUnit::GetDamage()
{
	if (circuitDef == nullptr) {  // unknown enemy is a threat
		return 0.1f;
	}
	float dmg = circuitDef->GetThrDamage();
	if (dmg < 1e-3f) {
		return .0f;
	}
	if (unit->IsParalyzed() || unit->IsBeingBuilt() || IsDisarmed()) {
		return 1e-3f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return dmg;
}

float CEnemyUnit::GetShieldPower() const
{
	if (circuitDef == nullptr) {
		return .0f;
	}
	return circuitDef->GetMaxShield();
}

void CEnemyUnit::SetNewPos(const AIFloat3& p) {
	newPos = p;
	CTerrainData::CorrectPosition(newPos);
}

} // namespace circuit
