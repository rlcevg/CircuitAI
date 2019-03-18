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

namespace circuit {

class IFighterTask;

class CEnemyUnit: public ICoreUnit {
public:
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

	void SetCost(float value) { cost = value; }
	float GetCost() const { return cost; }

	bool IsDisarmed();
	bool IsAttacker();
	float GetDamage();
	float GetShieldPower() const;

	void SetPos(const springai::AIFloat3& p) { pos = p; }
	const springai::AIFloat3& GetPos() const { return pos; }
	void SetNewPos(const springai::AIFloat3& p);
	const springai::AIFloat3& GetNewPos() const { return newPos; }

	void SetThreat(float t) { threat = t; }
	float GetThreat() const { return threat; }
	void DecayThreat(float decay) { threat *= decay; }

	void SetRange(CCircuitDef::ThreatType t, int r) { range[static_cast<CCircuitDef::ThreatT>(t)] = r; }
	int GetRange(CCircuitDef::ThreatType t = CCircuitDef::ThreatType::MAX) const { return range[static_cast<CCircuitDef::ThreatT>(t)]; }

private:
	std::set<IFighterTask*> tasks;
	int lastSeen;

	float cost;
	springai::AIFloat3 pos;
	springai::AIFloat3 newPos;
	float threat;
	std::array<int, static_cast<CCircuitDef::ThreatT>(CCircuitDef::ThreatType::_SIZE_)> range;

	enum LosMask: char {NONE = 0x00, LOS = 0x01, RADAR = 0x02, HIDDEN = 0x04, KNOWN = 0x08};
	using LM = std::underlying_type<LosMask>::type;

	LM losStatus;
public:
	void SetInLOS() { losStatus |= LosMask::LOS; }
	void SetInRadar() { losStatus |= LosMask::RADAR; }
	void SetHidden() { losStatus |= LosMask::HIDDEN; }
	void SetKnown() { losStatus |= LosMask::KNOWN; }
	void ClearInLOS() { losStatus &= ~LosMask::LOS; }
	void ClearInRadar() { losStatus &= ~LosMask::RADAR; }
	void ClearHidden() { losStatus &= ~LosMask::HIDDEN; }
	bool IsInLOS() const { return losStatus & LosMask::LOS; }
	bool IsInRadar() const { return losStatus & LosMask::RADAR; }
	bool IsInRadarOrLOS() const { return losStatus & (LosMask::RADAR | LosMask::LOS); }
	bool NotInRadarAndLOS() const { return (losStatus & (LosMask::RADAR | LosMask::LOS)) == 0; }
	bool IsHidden() const { return losStatus & LosMask::HIDDEN; }
	bool IsKnown() const { return losStatus & LosMask::KNOWN; }
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ENEMYUNIT_H_
