/*
 * MexTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_MEXTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_MEXTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBMexTask: public IBuilderTask {
public:
	CBMexTask(ITaskManager* mgr, Priority priority,
			  CCircuitDef* buildDef, int spotId, const springai::AIFloat3& position,
			  SResource cost, int timeout);
	CBMexTask(ITaskManager* mgr);  // Load
	virtual ~CBMexTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;

protected:
	virtual void Cancel() override;

	virtual bool Execute(CCircuitUnit* unit) override;

	virtual bool Reevaluate(CCircuitUnit* unit) override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;

	virtual void SetBuildPos(const springai::AIFloat3& pos) override;

private:
	bool CheckLandBlock(CCircuitUnit* unit);
	bool CheckWaterBlock(CCircuitUnit* unit);

	virtual bool Load(std::istream& is) override;
	virtual void Save(std::ostream& os) const override;

	int spotId;
	int blockCount;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_MEXTASK_H_
