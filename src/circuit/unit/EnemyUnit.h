/*
 * EnemyUnit.h
 *
 *  Created on: Aug 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ENEMYUNIT_H_
#define SRC_CIRCUIT_UNIT_ENEMYUNIT_H_

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

	CCircuitDef* cdef;

	float shieldPower;
	float health;
	bool isBeingBuilt;
	bool isParalyzed;
	bool isDisarmed;

	springai::AIFloat3 pos;
	springai::AIFloat3 vel;
	float threat;
	RangeArray range;
};

/*
 * Per AllyTeam enemy information
 */
class CEnemyUnit: public ICoreUnit {
private:
	enum LosMask: char {NONE = 0x00, LOS = 0x01, RADAR = 0x02, HIDDEN = 0x04, DEAD = 0x08};
	using LM = std::underlying_type<LosMask>::type;

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

	void SetCost(float value) { cost = value; }
	float GetCost() const { return cost; }

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
	int GetRange(CCircuitDef::ThreatType t = CCircuitDef::ThreatType::MAX) const {
		return GetRange(data.range, t);
	}
	static int GetRange(const SEnemyData::RangeArray& range, CCircuitDef::ThreatType t = CCircuitDef::ThreatType::MAX) {
		return range[static_cast<CCircuitDef::ThreatT>(t)];
	}

	void UpdateInRadarData(const springai::AIFloat3& p);
	void UpdateInLosData();

private:
	void Init();

	int knownFrame;
	int lastSeen;

	springai::Weapon* shield;
	float cost;

	SEnemyData data;

	LM losStatus;
public:
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

#endif // SRC_CIRCUIT_UNIT_ENEMYUNIT_H_
