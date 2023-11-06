/*
 * SuperTask.h
 *
 *  Created on: Aug 12, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_STATIC_SUPERTASK_H_
#define SRC_CIRCUIT_TASK_STATIC_SUPERTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

class CSuperTask: public IFighterTask {
public:
	CSuperTask(IUnitModule* mgr);
	virtual ~CSuperTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;

private:
	int targetFrame;
	springai::AIFloat3 targetPos;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_STATIC_SUPERTASK_H_
