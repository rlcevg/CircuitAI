/*
 * UnitTask.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_UNITTASK_H_
#define SRC_CIRCUIT_TASK_UNITTASK_H_

#include "AIFloat3.h"

#include <set>

namespace circuit {

class CCircuitUnit;
class CCircuitAI;

class IUnitTask {  // CSquad, IAction
public:
	enum class Priority: char {LOW = 0, NORMAL = 1, HIGH = 2};
	enum class Type: char {IDLE, RETREAT, BUILDER, FACTORY, ATTACK, SCOUT};

protected:
	IUnitTask(CCircuitAI* circuit, Priority priority, Type type);
public:
	virtual ~IUnitTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);
	virtual void MarkCompleted();

	virtual void Execute(CCircuitUnit* unit) = 0;  // <=> IAction::OnStart()
	virtual void Update() = 0;

	virtual void OnUnitIdle(CCircuitUnit* unit) = 0;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker) = 0;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker) = 0;

	const std::set<CCircuitUnit*>& GetAssignees() const;
	Priority GetPriority();
	Type GetType();

protected:
	CCircuitAI* circuit;
	std::set<CCircuitUnit*> units;
	Priority priority;
	Type type;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_UNITTASK_H_
