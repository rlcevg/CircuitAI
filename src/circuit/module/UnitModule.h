/*
 * UnitModule.h
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_UNITMODULE_H_
#define SRC_CIRCUIT_MODULE_UNITMODULE_H_

#include "module/Module.h"

#include <vector>

namespace circuit {

class IUnitTask;
class CCircuitAI;
class CCircuitUnit;
class CNilTask;
class CIdleTask;
class CPlayerTask;
class CRetreatTask;

class IUnitModule: public IModule {  // CActionList; UnitManager and TaskManager
public:
	enum class UseAs: char {
		COMBAT = 0, FENCE, SUPER, STOCK,  // military
		BUILDER, REZZER,  // builder
		FACTORY, ASSIST  // factory
	};

protected:
	IUnitModule(CCircuitAI* circuit, IScript* script);
public:
	virtual ~IUnitModule();

	void Init();
	void Release();

	void AssignTask(CCircuitUnit* unit, IUnitTask* task);
	void AssignTask(CCircuitUnit* unit);
protected:
	virtual void DequeueTask(IUnitTask* task, bool done = false);

public:
	virtual void FallbackTask(CCircuitUnit* unit) {}
	void AbortTask(IUnitTask* task) { DequeueTask(task, false); }
	void DoneTask(IUnitTask* task) { DequeueTask(task, true); }

public:
	// callins
	virtual IUnitTask* MakeTask(CCircuitUnit* unit);
	void TaskAdded(IUnitTask* task);
	void TaskRemoved(IUnitTask* task, bool done);
	void UnitAdded(CCircuitUnit* unit, UseAs usage);
	void UnitRemoved(CCircuitUnit* unit, UseAs usage);

	// callouts
	virtual IUnitTask* DefaultMakeTask(CCircuitUnit* unit) = 0;

public:
	CIdleTask* GetIdleTask() const { return idleTask; }
	virtual CRetreatTask* EnqueueRetreat() { return nullptr; }

	void AssignPlayerTask(CCircuitUnit* unit);
	void Resurrected(CCircuitUnit* unit);

	void AddMetalPull(float value) { metalPull += value; }
	void DelMetalPull(float value) { metalPull -= value; }
	float GetMetalPull() const { return metalPull; }

protected:
	void UpdateIdle();
	void Update();

	CNilTask* nilTask;
	CIdleTask* idleTask;
	CPlayerTask* playerTask;

	std::vector<IUnitTask*> updateTasks;  // owner
	unsigned int updateIterator;

	float metalPull;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_UNITMODULE_H_
