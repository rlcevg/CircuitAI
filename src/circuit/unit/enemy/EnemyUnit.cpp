/*
 * EnemyUnit.cpp
 *
 *  Created on: Aug 30, 2015
 *      Author: rlcevg
 */

#include "unit/enemy/EnemyUnit.h"
#include "task/fighter/FighterTask.h"
#include "util/Utils.h"

#include "WeaponMount.h"
#include "WrappWeapon.h"

namespace circuit {

using namespace springai;

CEnemyUnit::CEnemyUnit(Id unitId, Unit* unit, CCircuitDef* cdef)
		: ICoreUnit(unitId, unit)
		, knownFrame(-1)
		, lastSeen(-1)
		, data({cdef,
			0.f,             // shieldPower
			0.f,             // health
			false,           // isBeingBuilt
			false,           // isParalyzed
			false,           // isDisarmed
			ZeroVector,      // pos
			ZeroVector,      // vel
			0.f,             // threat
			{0},             // range
			unitId,          // id
			0.f,             // cost
			SEnemyData::LosMask::NONE})  // losStatus
{
	Init();
}

CEnemyUnit::~CEnemyUnit()
{
	delete shield;
}

void CEnemyUnit::SetCircuitDef(CCircuitDef* cdef)
{
	data.cdef = cdef;
	Init();
}

void CEnemyUnit::Init()
{
	if (data.cdef == nullptr) {
		data.cost = 0.f;
		shield = nullptr;
	} else {
		data.cost = data.cdef->GetCostM();
		WeaponMount* wpMnt = data.cdef->GetShieldMount();
		shield = (wpMnt == nullptr) ? nullptr : WrappWeapon::GetInstance(unit->GetSkirmishAIId(), id, wpMnt->GetWeaponMountId());
	}
}

float CEnemyUnit::GetRadius() const
{
	if (data.cdef == nullptr) {
		return 1.f;
	}
	return data.cdef->GetRadius();
}

bool CEnemyUnit::IsAttacker() const
{
	if (data.cdef == nullptr) {  // unknown enemy is a threat
		return true;
	}
	return data.cdef->IsAttacker();
}

float CEnemyUnit::GetDamage() const
{
	if (data.cdef == nullptr) {  // unknown enemy is a threat
		return 0.1f;
	}
	float dmg = data.cdef->GetThrDamage();
	if (dmg < 1e-3f) {
		return .0f;
	}
	if (data.isBeingBuilt || data.isParalyzed || data.isDisarmed) {
		return 1e-3f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return dmg;
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

CEnemyInfo::CEnemyInfo(CEnemyUnit* data)
		: data(data)
{
}

CEnemyInfo::~CEnemyInfo()
{
	for (IFighterTask* task : tasks) {
		task->ClearTarget();
	}
}

} // namespace circuit
