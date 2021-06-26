/*
 * MexUpTask.h
 *
 *  Created on: Jun 21, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_MEXUPTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_MEXUPTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBMexUpTask: public IBuilderTask {
public:
	CBMexUpTask(ITaskManager* mgr, Priority priority,
				CCircuitDef* buildDef, int spotId, const springai::AIFloat3& position,
				float cost, int timeout);
	virtual ~CBMexUpTask();

protected:
	virtual void Finish() override;
	virtual void Cancel() override;

	virtual void Execute(CCircuitUnit* unit) override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;

private:
	int spotId;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_MEXUPTASK_H_