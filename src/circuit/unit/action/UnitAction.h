/*
 * UnitAction.h
 *
 *  Created on: Jan 12, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_UNITACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_UNITACTION_H_

#include "util/Action.h"

#include <type_traits>

namespace circuit {

class CCircuitUnit;

class IUnitAction: public IAction {
public:
	enum class Type: int {IDLE, MOVE, PRE_BUILD, BUILD, ATTACK, FIGHT, PATROL, RECLAIM, TERRAFORM, WAIT, DGUN, JUMP, SUPPORT, ANTI_CAP, _SIZE_};
	enum Mask: int {IDLE      = 0x0001, MOVE  = 0x0002, PRE_BUILD = 0x0004, BUILD   = 0x0008,
					ATTACK    = 0x0010, FIGHT = 0x0020, PATROL    = 0x0040, RECLAIM = 0x0080,
					TERRAFORM = 0x0100, WAIT  = 0x0200, DGUN      = 0x0400, JUMP    = 0x0800,
					SUPPORT   = 0x1000};
	using T = std::underlying_type<Type>::type;
	using M = std::underlying_type<Mask>::type;

	static Mask GetMask(Type type) { return static_cast<Mask>(1 << static_cast<T>(type)); }

protected:
	IUnitAction(CCircuitUnit* owner, Type type);
public:
	virtual ~IUnitAction();

	bool IsAny(M value)     const { return (mask & value) != 0; }
	bool IsEqual(M value)   const { return mask == value; }
	bool IsContain(M value) const { return (mask & value) == value; }

protected:
	M mask;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_UNITACTION_H_
