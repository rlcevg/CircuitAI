/*
 * CircuitWDef.h
 *
 *  Created on: Oct 27, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_CIRCUITWDEF_H_
#define SRC_CIRCUIT_UNIT_CIRCUITWDEF_H_

#include "WeaponDef.h"

namespace circuit {

class CWeaponDef {
public:
	using Id = int;

//	CWeaponDef(const CWeaponDef& that) = delete;
	CWeaponDef& operator=(const CWeaponDef&) = delete;
	CWeaponDef(springai::WeaponDef* def, springai::Resource* resE);
	virtual ~CWeaponDef();

	static Id WeaponIdFromLua(int luaId);

	springai::WeaponDef* GetDef() const { return def; }

	float GetRange() const { return range; }
	float GetAoe() const { return aoe; }
	float GetCostE() const { return costE; }
	bool IsStockpile() const { return isStockpile; }
	bool IsHighTrajectory() const { return isHigh; }

private:
	springai::WeaponDef* def;  // owner

	float range;
	float aoe;
	float costE;
	bool isStockpile : 1;
	bool isHigh : 1;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITWDEF_H_
