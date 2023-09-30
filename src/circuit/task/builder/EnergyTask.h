/*
 * EnergyTask.h
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_ENERGYTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_ENERGYTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBEnergyTask: public IBuilderTask {
public:
	CBEnergyTask(ITaskManager* mgr, Priority priority,
				 CCircuitDef* buildDef, const springai::AIFloat3& position,
				 SResource cost, float shake, int timeout);
	CBEnergyTask(ITaskManager* mgr);  // Load
	virtual ~CBEnergyTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;

	virtual void Update() override;
protected:
	virtual void Finish() override;
	virtual void Cancel() override;

	virtual bool Reevaluate(CCircuitUnit* unit) override;

	virtual bool Load(std::istream& is) override;
	virtual void Save(std::ostream& os) const override;

private:
	bool isStalling;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_ENERGYTASK_H_
