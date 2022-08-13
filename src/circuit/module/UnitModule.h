/*
 * UnitModule.h
 *
 *  Created on: Jan 20, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_UNITMODULE_H_
#define SRC_CIRCUIT_MODULE_UNITMODULE_H_

#include "module/Module.h"
#include "unit/UnitManager.h"
#include "task/TaskManager.h"

namespace circuit {

class IUnitModule: public IModule, public IUnitManager, public ITaskManager {  // CActionList
protected:
	IUnitModule(CCircuitAI* circuit, IScript* script);
public:
	virtual ~IUnitModule();

	virtual CCircuitAI* GetCircuit() const override { return circuit; }

protected:
	virtual void DequeueTask(IUnitTask* task, bool done = false) override;

public:
	// callins
	virtual IUnitTask* MakeTask(CCircuitUnit* unit) override;
	void TaskCreated(IUnitTask* task);
	void TaskClosed(IUnitTask* task, bool done);

	// callouts
	virtual IUnitTask* DefaultMakeTask(CCircuitUnit* unit) = 0;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_UNITMODULE_H_
