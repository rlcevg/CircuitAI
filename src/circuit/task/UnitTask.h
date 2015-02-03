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
class ITaskManager;

class IUnitTask {  // CSquad, IAction
public:
	enum class Priority: char {LOW = 0, NORMAL = 1, HIGH = 2};
	enum class Type: char {PLAYER, IDLE, RETREAT, BUILDER, FACTORY, ATTACK, SCOUT};

protected:
	IUnitTask(ITaskManager* mgr, Priority priority, Type type);
public:
	virtual ~IUnitTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);
	virtual void Close(bool done);

	virtual void Execute(CCircuitUnit* unit) = 0;  // <=> IAction::OnStart()
	virtual void Update() = 0;
	// NOTE: Do not run time consuming code here. Instead create separate task.
	virtual void Finish();  // <=> IAction::OnEnd()

	virtual void OnUnitIdle(CCircuitUnit* unit) = 0;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker) = 0;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker) = 0;

	const std::set<CCircuitUnit*>& GetAssignees() const;
	Priority GetPriority();
	Type GetType();
	ITaskManager* GetManager();

protected:
	ITaskManager* manager;
	std::set<CCircuitUnit*> units;
	Priority priority;
	Type type;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_UNITTASK_H_
