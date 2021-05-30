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
class CNilTask;
class CIdleTask;
class CPlayerTask;
class CRetreatTask;

class ITaskManager {
protected:
	ITaskManager();
public:
	virtual ~ITaskManager();

	virtual CCircuitAI* GetCircuit() = 0;

	void AssignTask(CCircuitUnit* unit, IUnitTask* task);
	void AssignTask(CCircuitUnit* unit);
	virtual IUnitTask* MakeTask(CCircuitUnit*) = 0;
protected:
	virtual void DequeueTask(IUnitTask* task, bool done = false) = 0;
public:
	virtual void FallbackTask(CCircuitUnit* unit) = 0;
	void AbortTask(IUnitTask* task);
	void DoneTask(IUnitTask* task);

	void AddMetalPull(float value) { metalPull += value; }
	void DelMetalPull(float value) { metalPull -= value; }
	float GetMetalPull() const { return metalPull; }

	void Init();
	CIdleTask* GetIdleTask() const { return idleTask; }
	virtual CRetreatTask* EnqueueRetreat() { return nullptr; }

	void AssignPlayerTask(CCircuitUnit* unit);
	void Resurrected(CCircuitUnit* unit);

protected:
	CNilTask* nilTask;
	CIdleTask* idleTask;
	CPlayerTask* playerTask;

	float metalPull;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_TASKMANAGER_H_
