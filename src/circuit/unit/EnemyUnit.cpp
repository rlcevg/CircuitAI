/*
 * EnemyUnit.cpp
 *
 *  Created on: Aug 30, 2015
 *      Author: rlcevg
 */

#include "unit/EnemyUnit.h"
#include "task/fighter/FighterTask.h"
#include "util/utils.h"

#include "WeaponMount.h"
#include "WrappWeapon.h"

namespace circuit {

using namespace springai;

CEnemyUnit::CEnemyUnit(Id unitId, Unit* unit, CCircuitDef* cdef)
		: ICoreUnit(unitId, unit, cdef)
		, lastSeen(-1)
		, data({
			0.f,             // shieldPower
			0.f,             // health
			false,           // isBeingBuilt
			false,           // isParalyzed
			false,           // isDisarmed
			ZeroVector,      // pos
			ZeroVector,      // vel
			0.f,             // threat
			{0}})            // range
		, losStatus(LosMask::NONE)
{
	Init();
}

CEnemyUnit::~CEnemyUnit()
{
	for (IFighterTask* task : tasks) {
		task->ClearTarget();
	}
	delete shield;
}

void CEnemyUnit::Init()
{
	if (circuitDef == nullptr) {
		cost = 0.f;
		shield = nullptr;
	} else {
		cost = circuitDef->GetCost();
		WeaponMount* wpMnt = circuitDef->GetShieldMount();
		shield = (wpMnt == nullptr) ? nullptr : WrappWeapon::GetInstance(unit->GetSkirmishAIId(), id, wpMnt->GetWeaponMountId());
	}
}

void CEnemyUnit::SetCircuitDef(CCircuitDef* cdef)
{
	circuitDef = cdef;
	Init();
}

void CEnemyUnit::UpdateInRadarData(const AIFloat3& p)
{
	data.pos = p;
	CTerrainData::CorrectPosition(data.pos);
	data.vel = unit->GetVel();
}

void CEnemyUnit::UpdateInLosData()
{
	if (shield != nullptr) {
		data.shieldPower = shield->GetShieldPower();
	}
	data.health = unit->GetHealth();
	data.isParalyzed = unit->IsParalyzed();
	data.isBeingBuilt = unit->IsBeingBuilt();
	data.isDisarmed = unit->GetRulesParamFloat("disarmed", 0) > .0f;
}

bool CEnemyUnit::IsAttacker() const
{
	if (circuitDef == nullptr) {  // unknown enemy is a threat
		return true;
	}
	return circuitDef->IsAttacker();
}

float CEnemyUnit::GetDamage() const
{
	if (circuitDef == nullptr) {  // unknown enemy is a threat
		return 0.1f;
	}
	float dmg = circuitDef->GetThrDamage();
	if (dmg < 1e-3f) {
		return .0f;
	}
	if (data.isBeingBuilt || data.isParalyzed || data.isDisarmed) {
		return 1e-3f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return dmg;
}

} // namespace circuit
