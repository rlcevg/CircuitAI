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

class ITaskManager {
protected:
	ITaskManager();
public:
	virtual ~ITaskManager();

	virtual CCircuitAI* GetCircuit() = 0;

	void AssignTask(CCircuitUnit* unit, IUnitTask* task);
	void AssignTask(CCircuitUnit* unit);
	virtual IUnitTask* MakeTask(CCircuitUnit*) = 0;
	virtual void AbortTask(IUnitTask* task) = 0;
	virtual void DoneTask(IUnitTask* task) = 0;
	virtual void FallbackTask(CCircuitUnit* unit) = 0;

	void Init();
	CIdleTask* GetIdleTask() const { return idleTask; }

protected:
	CNullTask* nullTask;
	CIdleTask* idleTask;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_TASKMANAGER_H_
