/*
 * EnemyUnit.h
 *
 *  Created on: Aug 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ENEMYUNIT_H_
#define SRC_CIRCUIT_UNIT_ENEMYUNIT_H_

#include "unit/CircuitUnit.h"

#include <set>

namespace circuit {

class IFighterTask;

class CEnemyUnit {
public:
	enum class RangeType: char {MAX = 0, AIR = 1, LAND = 2, WATER = 3, CLOAK = 4, TOTAL_COUNT};
	using RT = std::underlying_type<RangeType>::type;

	CEnemyUnit(const CEnemyUnit& that) = delete;
	CEnemyUnit& operator=(const CEnemyUnit&) = delete;
	CEnemyUnit(springai::Unit* unit, CCircuitDef* cdef);
	virtual ~CEnemyUnit();

	CCircuitUnit::Id GetId() const { return id; }
	springai::Unit* GetUnit() const { return unit; }

	void SetCircuitDef(CCircuitDef* cdef);
	CCircuitDef* GetCircuitDef() const { return circuitDef; }

	void BindTask(IFighterTask* task) { tasks.insert(task); }
	void UnbindTask(IFighterTask* task) { tasks.erase(task); }

	void SetLastSeen(int frame) { lastSeen = frame; }
	int GetLastSeen() const { return lastSeen; }

	springai::Weapon* GetDGun() const { return dgun; }
	bool IsDisarmed();
	float GetDPS();

	void SetPos(const springai::AIFloat3& p) { pos = p; }
	const springai::AIFloat3& GetPos() const { return pos; }

	void SetThreat(float t) { threat = t; }
	float GetThreat() const { return threat; }
	void DecayThreat(float decay) { threat *= decay; }

	void SetRange(RangeType t, int r) { range[static_cast<RT>(t)] = r; }
	int GetRange(RangeType t = RangeType::MAX) const { return range[static_cast<RT>(t)]; }

	bool operator==(const CCircuitUnit& rhs) { return id == rhs.GetId(); }
	bool operator!=(const CCircuitUnit& rhs) { return id != rhs.GetId(); }

private:
	CCircuitUnit::Id id;
	springai::Unit* unit;  // owner
	CCircuitDef* circuitDef;
	std::set<IFighterTask*> tasks;
	int lastSeen;

	springai::Weapon* dgun;

	springai::AIFloat3 pos;
	float threat;
	std::array<int, static_cast<RT>(RangeType::TOTAL_COUNT)> range;

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
