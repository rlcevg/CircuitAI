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
	enum Type: int {IDLE      = 0x0001, MOVE  = 0x0002, PRE_BUILD = 0x0004, BUILD   = 0x0008,
					ATTACK    = 0x0010, FIGHT = 0x0020, PATROL    = 0x0040, RECLAIM = 0x0080,
					TERRAFORM = 0x0100, WAIT  = 0x0200, DGUN      = 0x0400, JUMP    = 0x0800};

protected:
	IUnitAction(CCircuitUnit* owner, Type type);
public:
	virtual ~IUnitAction();

	std::underlying_type<Type>::type GetType() const { return type; }

protected:
	std::underlying_type<circuit::IUnitAction::Type>::type type;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_UNITACTION_H_
