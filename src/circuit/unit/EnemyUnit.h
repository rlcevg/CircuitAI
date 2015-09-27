/*
 * EnemyUnit.h
 *
 *  Created on: Aug 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ENEMYUNIT_H_
#define SRC_CIRCUIT_UNIT_ENEMYUNIT_H_

#include "unit/CircuitUnit.h"

namespace circuit {

class CEnemyUnit {
public:
	CEnemyUnit(const CEnemyUnit& that) = delete;
	CEnemyUnit& operator=(const CEnemyUnit&) = delete;
	CEnemyUnit(springai::Unit* unit, CCircuitDef* cdef);
	virtual ~CEnemyUnit();

	CCircuitUnit::Id GetId() const { return id; }
	springai::Unit* GetUnit() const { return unit; }

	void SetCircuitDef(CCircuitDef* cdef);
	CCircuitDef* GetCircuitDef() const { return circuitDef; }

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

	void SetRange(int r) { range = r; }
	int GetRange() const { return range; }

	void SetDecloakRange(int r) { rangeDecloak = r; }
	int GetDecloakRange() const { return rangeDecloak; }

	bool operator==(const CCircuitUnit& rhs) { return id == rhs.GetId(); }
	bool operator!=(const CCircuitUnit& rhs) { return id != rhs.GetId(); }

private:
	CCircuitUnit::Id id;
	springai::Unit* unit;  // owner
	CCircuitDef* circuitDef;

	int lastSeen;

	springai::Weapon* dgun;
	springai::UnitRulesParam* disarmParam;

	enum LosType: char {NONE = 0x00, LOS = 0x01, RADAR = 0x02, HIDDEN = 0x04};
	springai::AIFloat3 pos;
	float threat;
	int range;
	int rangeDecloak;
	std::underlying_type<LosType>::type losStatus;

public:
	void SetInLOS() { losStatus |= LosType::LOS; }
	void SetInRadar() { losStatus |= LosType::RADAR; }
	void SetHidden() { losStatus |= LosType::HIDDEN; }
	void ClearInLOS() { losStatus &= ~LosType::LOS; }
	void ClearInRadar() { losStatus &= ~LosType::RADAR; }
	void ClearHidden() { losStatus &= ~LosType::HIDDEN; }
	bool IsInLOS() const { return losStatus & LosType::LOS; }
	bool IsInRadar() const { return losStatus & LosType::RADAR; }
	bool IsInRadarOrLOS() const { return losStatus & (LosType::RADAR | LosType::LOS); }
	bool NotInRadarAndLOS() const { return (losStatus & (LosType::RADAR | LosType::LOS)) == 0; }
	bool IsHidden() const { return losStatus & LosType::HIDDEN; }
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ENEMYUNIT_H_
