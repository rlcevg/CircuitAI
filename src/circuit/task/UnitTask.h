/*
 * UnitTask.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef UNITTASK_H_
#define UNITTASK_H_

#include "util/ActionList.h"

#include "AIFloat3.h"

#include <set>

namespace circuit {

class CCircuitUnit;

class IUnitTask: public CActionList {  // CSquad, IAction
public:
	enum class Priority: char {LOW = 0, NORMAL = 1, HIGH = 2};

protected:
	IUnitTask(Priority priority);
public:
	virtual ~IUnitTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);
	virtual void MarkCompleted();

	// TODO: Remove pure virtual?
	virtual void OnUnitIdle(CCircuitUnit* unit) = 0;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker) = 0;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker) = 0;

	std::set<CCircuitUnit*>& GetAssignees();
	Priority GetPriority();

protected:
	std::set<CCircuitUnit*> units;
	Priority priority;
};

} // namespace circuit

#endif // UNITTASK_H_
