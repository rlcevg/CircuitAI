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
struct SEnemyData {
	using RangeArray = std::array<int, static_cast<CCircuitDef::ThreatT>(CCircuitDef::ThreatType::_SIZE_)>;

	enum LosMask: char {NONE = 0x00, LOS = 0x01, RADAR = 0x02, HIDDEN = 0x04, DEAD = 0x08};
	using LM = std::underlying_type<LosMask>::type;

	CCircuitDef* cdef;

	float shieldPower;
	float health;
	bool isBeingBuilt;
	bool isParalyzed;
	bool isDisarmed;

	springai::AIFloat3 pos;
	springai::AIFloat3 vel;  // elmos per frame
	float threat;
	RangeArray range;

	ICoreUnit::Id id;  // FIXME: duplicate
	float cost;
	LM losStatus;

	void SetInLOS()     { losStatus |= LosMask::LOS; }
	void SetInRadar()   { losStatus |= LosMask::RADAR; }
	void SetHidden()    { losStatus |= LosMask::HIDDEN; }
	void Dead()         { losStatus |= LosMask::DEAD | LosMask::HIDDEN; }
	void ClearInLOS()   { losStatus &= ~LosMask::LOS; }
	void ClearInRadar() { losStatus &= ~LosMask::RADAR; }
	void ClearHidden()  { losStatus &= ~LosMask::HIDDEN; }

	bool IsInLOS()          const { return losStatus & LosMask::LOS; }
	bool IsInRadar()        const { return losStatus & LosMask::RADAR; }
	bool IsInRadarOrLOS()   const { return losStatus & (LosMask::RADAR | LosMask::LOS); }
	bool NotInRadarAndLOS() const { return (losStatus & (LosMask::RADAR | LosMask::LOS)) == 0; }
	bool IsHidden()         const { return losStatus & LosMask::HIDDEN; }
	bool IsDead()           const { return losStatus & LosMask::DEAD; }
};

/*
 * Per AllyTeam enemy information
 */
class CEnemyUnit: public ICoreUnit {
public:
	CEnemyUnit(const CEnemyUnit& that) = delete;
	CEnemyUnit& operator=(const CEnemyUnit&) = delete;
	CEnemyUnit(Id unitId, springai::Unit* unit, CCircuitDef* cdef);
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

	bool IsAttacker() const;
	float GetDamage() const;
	float GetShieldPower() const { return data.shieldPower; }

	const springai::AIFloat3& GetPos() const { return data.pos; }
	const springai::AIFloat3& GetVel() const { return data.vel; }

	void SetThreat(float t) { data.threat = t; }
	float GetThreat() const { return data.threat; }

	void SetRange(CCircuitDef::ThreatType t, int r) {
		data.range[static_cast<CCircuitDef::ThreatT>(t)] = r;
	}
	int GetRange(CCircuitDef::ThreatType t) const {
		return GetRange(data.range, t);
	}
	static int GetRange(const SEnemyData::RangeArray& range, CCircuitDef::ThreatType t) {
		return range[static_cast<CCircuitDef::ThreatT>(t)];
	}

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
	void Dead()         { data.Dead(); }
	void ClearInLOS()   { data.ClearInLOS(); }
	void ClearInRadar() { data.ClearInRadar(); }
	void ClearHidden()  { data.ClearHidden(); }

	bool IsInLOS()          const { return data.IsInLOS(); }
	bool IsInRadar()        const { return data.IsInRadar(); }
	bool IsInRadarOrLOS()   const { return data.IsInRadarOrLOS(); }
	bool NotInRadarAndLOS() const { return data.NotInRadarAndLOS(); }
	bool IsHidden()         const { return data.IsHidden(); }
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
	float GetDamage() const { return data->GetDamage(); }
	float GetShieldPower() const { return data->GetShieldPower(); }

	const springai::AIFloat3& GetPos() const { return data->GetPos(); }
	const springai::AIFloat3& GetVel() const { return data->GetVel(); }

	float GetThreat() const { return data->GetThreat(); }

	bool IsInLOS()          const { return data->IsInLOS(); }
	bool IsInRadarOrLOS()   const { return data->IsInRadarOrLOS(); }
	bool NotInRadarAndLOS() const { return data->NotInRadarAndLOS(); }
	bool IsHidden()         const { return data->IsHidden(); }

	CEnemyUnit* GetData() const { return data; }

private:
	std::set<IFighterTask*> tasks;

	CEnemyUnit* data;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ENEMY_ENEMYUNIT_H_
