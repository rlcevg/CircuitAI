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

	virtual CCircuitAI* GetCircuit() const override { return circuit; }

protected:
	virtual void DequeueTask(IUnitTask* task, bool done = false) override;

public:
	// callins
	virtual IUnitTask* MakeTask(CCircuitUnit* unit) override;
	void TaskAdded(IUnitTask* task);
	void TaskRemoved(IUnitTask* task, bool done);
	void UnitAdded(CCircuitUnit* unit, UseAs usage);
	void UnitRemoved(CCircuitUnit* unit, UseAs usage);

	// callouts
	virtual IUnitTask* DefaultMakeTask(CCircuitUnit* unit) = 0;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_UNITMODULE_H_
