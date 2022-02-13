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
using namespace terrain;

float SEnemyData::GetDefDamage() const
{
	if (cdef == nullptr) {  // unknown enemy is a threat
		return 0.1f;
	}
	const float dmg = cdef->GetDefDamage();
	if (dmg < 1e-3f) {
		return .0f;
	}
	if (isBeingBuilt || isParalyzed || isDisarmed) {
		return 1e-3f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return dmg;
}

float SEnemyData::GetAirDamage(CCircuitDef::RoleT type) const
{
	if (cdef == nullptr) {  // unknown enemy is a threat
		return 0.1f;
	}
	if (cdef->GetDefDamage() < 1e-3f) {
		return .0f;
	}
	if (isBeingBuilt || isParalyzed || isDisarmed) {
		return 1e-3f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return cdef->GetAirDmg(type);
}

float SEnemyData::GetSurfDamage(CCircuitDef::RoleT type) const
{
	if (cdef == nullptr) {  // unknown enemy is a threat
		return 0.1f;
	}
	if (cdef->GetDefDamage() < 1e-3f) {
		return .0f;
	}
	if (isBeingBuilt || isParalyzed || isDisarmed) {
		return 1e-3f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return cdef->GetSurfDmg(type);
}

float SEnemyData::GetWaterDamage(CCircuitDef::RoleT type) const
{
	if (cdef == nullptr) {  // unknown enemy is a threat
		return 0.1f;
	}
	if (cdef->GetDefDamage() < 1e-3f) {
		return .0f;
	}
	if (isBeingBuilt || isParalyzed || isDisarmed) {
		return 1e-3f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return cdef->GetWaterDmg(type);
}

CEnemyUnit::CEnemyUnit(Id unitId, Unit* unit, CCircuitDef* cdef)
		: ICoreUnit(unitId, unit)
		, knownFrame(-1)
		, lastSeen(-1)
		, data(SEnemyData {.cdef = cdef,
			.shieldPower = 0.f,
			.health = 0.f,
			.isBeingBuilt = false,
			.isParalyzed = false,
			.isDisarmed = false,
			.pos = ZeroVector,
			.vel = ZeroVector,
			.thrPos = ZeroVector,
			.thrHealth = 1.f,
			.range = {0},
			.influence = 0.f,
			.id = unitId,
			.cost = 0.f,
			.losStatus = SEnemyData::LosMask::NONE})
{
	Init();
}

CEnemyUnit::CEnemyUnit(CCircuitDef* cdef, const AIFloat3& pos)
		: ICoreUnit(-1, nullptr)
		, knownFrame(-1)
		, lastSeen(-1)
		, shield(nullptr)
		, data(SEnemyData {.cdef = cdef,
			.shieldPower = cdef->GetMaxShield(),
			.health = cdef->GetHealth(),
			.isBeingBuilt = false,
			.isParalyzed = false,
			.isDisarmed = false,
			.pos = pos,
			.vel = ZeroVector,
			.thrPos = ZeroVector,
			.thrHealth = 1.f,
			.range = {0},
			.influence = 0.f,
			.id = -1,
			.cost = cdef->GetCostM(),
			.losStatus = SEnemyData::LosMask::NONE})
{
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

void CEnemyUnit::ClearThreat()
{
	SetInfluence(0.f);
	data.thrHealth = 1.f;
}

void CEnemyUnit::UpdateInRadarData(const AIFloat3& p)
{
	data.pos = p;
	CTerrainData::CorrectPosition(data.pos);

	// IsNeutral works in los or radar, @see rts/ExternalAI/AICallback.cpp CAICallback::IsUnitNeutral
	unit->IsNeutral() ? SetNeutral() : ClearNeutral();
	if (IsIgnore()) {
		return;
	}

	data.vel = unit->GetVel();
}

void CEnemyUnit::UpdateInLosData()
{
	if (IsIgnore()) {
		return;
	}

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

CEnemyFake::CEnemyFake(CCircuitDef* cdef, const AIFloat3& pos, int timeout)
		: CEnemyUnit(cdef, pos)
		, timeout(timeout)
{
}

CEnemyFake::~CEnemyFake()
{
}

} // namespace circuit
