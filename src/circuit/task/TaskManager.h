/*
 * TaskManager.h
 *
 *  Created on: Feb 1, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_TASKMANAGER_H_
#define SRC_CIRCUIT_TASK_TASKMANAGER_H_

namespace circuit {

class IUnitTask;
class CCircuitAI;
class CCircuitUnit;
class CNullTask;
class CIdleTask;
class CRetreatTask;

class ITaskManager {
protected:
	ITaskManager();
public:
	virtual ~ITaskManager();

	virtual CCircuitAI* GetCircuit() = 0;

	void AssignTask(CCircuitUnit* unit, IUnitTask* task);
	virtual void AssignTask(CCircuitUnit* unit) = 0;
	virtual void AbortTask(IUnitTask* task) = 0;
	virtual void DoneTask(IUnitTask* task) = 0;
	virtual void FallbackTask(CCircuitUnit* unit) = 0;

	CIdleTask* GetIdleTask() const { return idleTask; }
	CRetreatTask* GetRetreatTask() const { return retreatTask; }

protected:
	CNullTask* nullTask;
	CIdleTask* idleTask;
	CRetreatTask* retreatTask;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_TASKMANAGER_H_
