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
	CBMexUpTask(ITaskManager* mgr);  // Load
	virtual ~CBMexUpTask();

protected:
	virtual void Finish() override;
	virtual void Cancel() override;

	virtual void Execute(CCircuitUnit* unit) override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;

private:
	virtual void Load(std::istream& is) override;
	virtual void Save(std::ostream& os) const override;

	int spotId;
	CCircuitUnit* reclaimMex;  // NOTE: never use it as unit, it's a mark (void*)
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_MEXUPTASK_H_
