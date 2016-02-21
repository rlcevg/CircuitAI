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
	enum class RangeType: char {MAX = 0, AIR = 1, LAND = 2, WATER = 3, CLOAK = 4, COUNT};

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

	void SetRange(RangeType t, int r) { range[static_cast<unsigned>(t)] = r; }
	int GetRange(RangeType t = CEnemyUnit::RangeType::MAX) const { return range[static_cast<unsigned>(t)]; }

	bool operator==(const CCircuitUnit& rhs) { return id == rhs.GetId(); }
	bool operator!=(const CCircuitUnit& rhs) { return id != rhs.GetId(); }

private:
	CCircuitUnit::Id id;
	springai::Unit* unit;  // owner
	CCircuitDef* circuitDef;
	std::set<IFighterTask*> tasks;
	int lastSeen;

	springai::Weapon* dgun;
	springai::UnitRulesParam* disarmParam;

	enum LosType: char {NONE = 0x00, LOS = 0x01, RADAR = 0x02, HIDDEN = 0x04, KNOWN = 0x08};
	springai::AIFloat3 pos;
	float threat;
	std::array<int, static_cast<unsigned>(RangeType::COUNT)> range;

	std::underlying_type<LosType>::type losStatus;
public:
	void SetInLOS() { losStatus |= LosType::LOS; }
	void SetInRadar() { losStatus |= LosType::RADAR; }
	void SetHidden() { losStatus |= LosType::HIDDEN; }
	void SetKnown() { losStatus |= LosType::KNOWN; }
	void ClearInLOS() { losStatus &= ~LosType::LOS; }
	void ClearInRadar() { losStatus &= ~LosType::RADAR; }
	void ClearHidden() { losStatus &= ~LosType::HIDDEN; }
	bool IsInLOS() const { return losStatus & LosType::LOS; }
	bool IsInRadar() const { return losStatus & LosType::RADAR; }
	bool IsInRadarOrLOS() const { return losStatus & (LosType::RADAR | LosType::LOS); }
	bool NotInRadarAndLOS() const { return (losStatus & (LosType::RADAR | LosType::LOS)) == 0; }
	bool IsHidden() const { return losStatus & LosType::HIDDEN; }
	bool IsKnown() const { return losStatus & LosType::KNOWN; }
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ENEMYUNIT_H_
