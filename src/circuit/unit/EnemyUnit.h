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

class CEnemyUnit: public ICoreUnit {
private:
	enum LosMask: char {NONE = 0x00, LOS = 0x01, RADAR = 0x02, HIDDEN = 0x04, KNOWN = 0x08,
									DEAD = 0x10};
	using LM = std::underlying_type<LosMask>::type;
	using RangeArray = std::array<int, static_cast<CCircuitDef::ThreatT>(CCircuitDef::ThreatType::_SIZE_)>;

public:
	struct SData {
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
	struct SEnemyData {
		CCircuitDef* cdef;
		SData data;
	};

	CEnemyUnit(const CEnemyUnit& that) = delete;
	CEnemyUnit& operator=(const CEnemyUnit&) = delete;
	CEnemyUnit(Id unitId, springai::Unit* unit, CCircuitDef* cdef);
	virtual ~CEnemyUnit();

	void SetCircuitDef(CCircuitDef* cdef);

	void BindTask(IFighterTask* task) { tasks.insert(task); }
	void UnbindTask(IFighterTask* task) { tasks.erase(task); }
	const std::set<IFighterTask*>& GetTasks() const { return tasks; }

	void SetLastSeen(int frame) { lastSeen = frame; }
	int GetLastSeen() const { return lastSeen; }

	void UpdateInRadarData(const springai::AIFloat3& p);
	void UpdateInLosData();

	float GetHealth() const { return data.health; }
	bool IsBeingBuilt() const { return data.isBeingBuilt; }
	bool IsParalyzed() const { return data.isParalyzed; }
	bool IsDisarmed() const { return data.isDisarmed; }

	void SetCost(float value) { cost = value; }
	float GetCost() const { return cost; }

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
	static int GetRange(const RangeArray& range, CCircuitDef::ThreatType t = CCircuitDef::ThreatType::MAX) {
		return range[static_cast<CCircuitDef::ThreatT>(t)];
	}

private:
	void Init();

	std::set<IFighterTask*> tasks;
	int lastSeen;

	springai::Weapon* shield;
	float cost;

	SData data;

	LM losStatus;
public:
	void SetInLOS()     { losStatus |= LosMask::LOS; }
	void SetInRadar()   { losStatus |= LosMask::RADAR; }
	void SetHidden()    { losStatus |= LosMask::HIDDEN; }
	void SetKnown()     { losStatus |= LosMask::KNOWN; }
	void Dead()         { losStatus |= LosMask::DEAD | LosMask::HIDDEN; }
	void ClearInLOS()   { losStatus &= ~LosMask::LOS; }
	void ClearInRadar() { losStatus &= ~LosMask::RADAR; }
	void ClearHidden()  { losStatus &= ~LosMask::HIDDEN; }

	bool IsInLOS()          const { return losStatus & LosMask::LOS; }
	bool IsInRadar()        const { return losStatus & LosMask::RADAR; }
	bool IsInRadarOrLOS()   const { return losStatus & (LosMask::RADAR | LosMask::LOS); }
	bool NotInRadarAndLOS() const { return (losStatus & (LosMask::RADAR | LosMask::LOS)) == 0; }
	bool IsHidden()         const { return losStatus & LosMask::HIDDEN; }
	bool IsKnown()          const { return losStatus & LosMask::KNOWN; }
	bool IsDead()           const { return losStatus & LosMask::DEAD; }

	const SEnemyData GetData() const { return {circuitDef, data}; }
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ENEMYUNIT_H_
