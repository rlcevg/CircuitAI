/*
 * EnemyUnit.h
 *
 *  Created on: Aug 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ENEMY_ENEMYUNIT_H_
#define SRC_CIRCUIT_UNIT_ENEMY_ENEMYUNIT_H_

#include "unit/CoreUnit.h"
#include "unit/CircuitDef.h"

#include <set>

namespace springai {
	class Weapon;
}

namespace circuit {

class IFighterTask;

/*
 * Data only structure ease of copy (double-buffer)
 */
#undef IGNORE  // FIXME: mingw64-gcc5.4 workaround
struct SEnemyData {
	using RangeArray = std::array<int, static_cast<CCircuitDef::ThreatT>(CCircuitDef::ThreatType::_SIZE_)>;

	enum LosMask: char {NONE   = 0x00,
						LOS    = 0x01, RADAR = 0x02, HIDDEN = 0x04, NEUTRAL = 0x08,
						IGNORE = 0x08, DYING = 0x20, DEAD   = 0x40};
	using LM = std::underlying_type<LosMask>::type;

	CCircuitDef* cdef;

	float shieldPower;
	float health;
	bool isBeingBuilt;
	bool isParalyzed;
	bool isDisarmed;

	springai::AIFloat3 pos;
	springai::AIFloat3 vel;  // elmos per frame
	springai::AIFloat3 thrPos;  // pos shifted by vel, used only for threat drawing
	float thrHealth;  // health-based part
	RangeArray range;
	float influence;

	float GetDefDamage() const;
	float GetAirDamage(CCircuitDef::RoleT type) const;
	float GetSurfDamage(CCircuitDef::RoleT type) const;
	float GetWaterDamage(CCircuitDef::RoleT type) const;

	void SetRange(CCircuitDef::ThreatType type, int value) {
		range[static_cast<CCircuitDef::ThreatT>(type)] = value;
	}
	int GetRange(CCircuitDef::ThreatType type) const {
		return range[static_cast<CCircuitDef::ThreatT>(type)];
	}

	bool IsFake() const { return id == -1; }

	ICoreUnit::Id id;  // FIXME: duplicate
	float cost;
	LM losStatus;

	void SetInLOS()     { losStatus |= LosMask::LOS; }
	void SetInRadar()   { losStatus |= LosMask::RADAR; }
	void SetHidden()    { losStatus |= LosMask::HIDDEN; }
	void SetNeutral()   { losStatus |= LosMask::NEUTRAL; }
	void SetIgnore()    { losStatus |= LosMask::IGNORE; }
	void SetDying()     { losStatus |= LosMask::DYING | LosMask::HIDDEN; }
	void SetDead()      { losStatus |= LosMask::DEAD | LosMask::HIDDEN; }
	void ClearInLOS()   { losStatus &= ~LosMask::LOS; }
	void ClearInRadar() { losStatus &= ~LosMask::RADAR; }
	void ClearHidden()  { losStatus &= ~LosMask::HIDDEN; }
	void ClearNeutral() { losStatus &= ~LosMask::NEUTRAL; };
	void ClearIgnore()  { losStatus &= ~LosMask::IGNORE; }

	bool IsInLOS()          const { return losStatus & LosMask::LOS; }
	bool IsInRadar()        const { return losStatus & LosMask::RADAR; }
	bool IsInRadarOrLOS()   const { return losStatus & (LosMask::RADAR | LosMask::LOS); }
	bool NotInRadarAndLOS() const { return (losStatus & (LosMask::RADAR | LosMask::LOS)) == 0; }
	bool IsHidden()         const { return losStatus & (LosMask::HIDDEN | LosMask::NEUTRAL | LosMask::IGNORE); }
	bool IsNeutral()        const { return losStatus & LosMask::NEUTRAL; }
	bool IsIgnore()         const { return losStatus & (LosMask::IGNORE | LosMask::NEUTRAL); }
	bool IsDying()          const { return losStatus & LosMask::DYING; }
	bool IsDead()           const { return losStatus & LosMask::DEAD; }
};

/*
 * Per AllyTeam common enemy information
 */
class CEnemyUnit: public ICoreUnit {
	friend class CEnemyManager;  // FIXME: HAX to re-create WrappUnit::GetInstance by authority AI
public:
	CEnemyUnit(const CEnemyUnit& that) = delete;
	CEnemyUnit& operator=(const CEnemyUnit&) = delete;
	CEnemyUnit(Id unitId, springai::Unit* unit, CCircuitDef* cdef);
	CEnemyUnit(CCircuitDef* cdef, const springai::AIFloat3& pos);
	virtual ~CEnemyUnit();

	void SetCircuitDef(CCircuitDef* cdef);
	CCircuitDef* GetCircuitDef() const { return data.cdef; }

	void SetKnown(int frame) { knownFrame = (knownFrame != -1) ? knownFrame : frame; }
	bool IsKnown(int frame) const { return (knownFrame != -1) && (knownFrame != frame); }

	void SetLastSeen(int frame) { lastSeen = frame; }
	int GetLastSeen() const { return lastSeen; }

	void SetCost(float value) { data.cost = value; }
	float GetCost() const { return data.cost; }

	float GetRadius() const;
	float GetHealth() const { return data.health; }
	bool IsBeingBuilt() const { return data.isBeingBuilt; }
	bool IsParalyzed() const { return data.isParalyzed; }
	bool IsDisarmed() const { return data.isDisarmed; }
	// Next functions used by FakeEnemy system
	void SetHealth(float value) { data.health = value; }
	void SetBeingBuilt(bool value) { data.isBeingBuilt = value; }

	bool IsAttacker() const;
	float GetDefDamage() const { return data.GetDefDamage(); }
	float GetShieldPower() const { return data.shieldPower; }

	const springai::AIFloat3& GetPos() const { return data.pos; }
	const springai::AIFloat3& GetVel() const { return data.vel; }

	void SetInfluence(float value) { data.influence = value; }
	float GetInfluence() const { return data.influence; }
	void SetThrHealth(float value) { data.thrHealth = value; }
	void ClearThreat();
//	float GetThreat(CCircuitDef::RoleT type) const { return data.GetDamage(type) * data.thrMod; }

	void SetRange(CCircuitDef::ThreatType type, int value) { data.SetRange(type, value); }
	int GetRange(CCircuitDef::ThreatType type) const { return data.GetRange(type); }

	bool IsFake() const { return data.IsFake(); }

	void UpdateInRadarData(const springai::AIFloat3& p);
	void UpdateInLosData();

private:
	void Init();

	int knownFrame;
	int lastSeen;

	springai::Weapon* shield;

	SEnemyData data;

public:
	void SetInLOS()     { data.SetInLOS(); }
	void SetInRadar()   { data.SetInRadar(); }
	void SetHidden()    { data.SetHidden(); }
	void SetNeutral()   { data.SetNeutral(); }
	void SetIgnore()    { data.SetIgnore(); }
	void SetDying()     { data.SetDying(); }
	void SetDead()      { data.SetDead(); }
	void ClearInLOS()   { data.ClearInLOS(); }
	void ClearInRadar() { data.ClearInRadar(); }
	void ClearHidden()  { data.ClearHidden(); }
	void ClearNeutral() { data.ClearNeutral(); }
	void ClearIgnore()  { data.ClearIgnore(); }  // on decoy identification

	bool IsInLOS()          const { return data.IsInLOS(); }
	bool IsInRadar()        const { return data.IsInRadar(); }
	bool IsInRadarOrLOS()   const { return data.IsInRadarOrLOS(); }
	bool NotInRadarAndLOS() const { return data.NotInRadarAndLOS(); }
	bool IsHidden()         const { return data.IsHidden(); }
	bool IsNeutral()        const { return data.IsNeutral(); }
	bool IsIgnore()         const { return data.IsIgnore(); }
	bool IsDying()          const { return data.IsDying(); }
	bool IsDead()           const { return data.IsDead(); }

	const SEnemyData GetData() const { return data; }
};

/*
 * Per AI enemy information, bridge to connect tasks and common enemy data
 */
class CEnemyInfo {
public:
	CEnemyInfo(const CEnemyInfo& that) = delete;
	CEnemyInfo& operator=(const CEnemyInfo&) = delete;
	CEnemyInfo(CEnemyUnit* data);
	virtual ~CEnemyInfo();

	void BindTask(IFighterTask* task) { tasks.insert(task); }
	void UnbindTask(IFighterTask* task) { tasks.erase(task); }
	const std::set<IFighterTask*>& GetTasks() const { return tasks; }

	ICoreUnit::Id GetId() const { return data->GetId(); }
	springai::Unit* GetUnit() const { return data->GetUnit(); }
	CCircuitDef* GetCircuitDef() const { return data->GetCircuitDef(); }

	float GetCost() const { return data->GetCost(); }

	float GetHealth() const { return data->GetHealth(); }
	bool IsBeingBuilt() const { return data->IsBeingBuilt(); }
	bool IsParalyzed() const { return data->IsParalyzed(); }
	bool IsDisarmed() const { return data->IsDisarmed(); }

	bool IsAttacker() const { return data->IsAttacker(); }
	float GetShieldPower() const { return data->GetShieldPower(); }

	const springai::AIFloat3& GetPos() const { return data->GetPos(); }
	const springai::AIFloat3& GetVel() const { return data->GetVel(); }

	float GetInfluence() const { return data->GetInfluence(); }
//	float GetThreat(CCircuitDef::RoleT type, float surfMod) const { return data->GetThreat(type) * surfMod; }

	bool IsInLOS()          const { return data->IsInLOS(); }
	bool IsInRadarOrLOS()   const { return data->IsInRadarOrLOS(); }
	bool NotInRadarAndLOS() const { return data->NotInRadarAndLOS(); }
	bool IsHidden()         const { return data->IsHidden(); }

	CEnemyUnit* GetData() const { return data; }

private:
	std::set<IFighterTask*> tasks;

	CEnemyUnit* data;
};

/*
 * Per AllyTeam fake ghost enemy
 */
class CEnemyFake: public CEnemyUnit {
public:
	CEnemyFake(const CEnemyInfo& that) = delete;
	CEnemyFake& operator=(const CEnemyFake&) = delete;
	CEnemyFake(CCircuitDef* cdef, const springai::AIFloat3& pos, int timeout);
	virtual ~CEnemyFake();

	int GetTimeout() const { return timeout; }

private:
	int timeout;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ENEMY_ENEMYUNIT_H_
