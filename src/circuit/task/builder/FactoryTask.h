/*
 * FactoryTask.h
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FACTORYTASK_H_
#define SRC_CIRCUIT_TASK_FACTORYTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBFactoryTask: public IBuilderTask {
public:
	CBFactoryTask(ITaskManager* mgr, Priority priority,
				  CCircuitDef* buildDef, CCircuitDef* reprDef, const springai::AIFloat3& position,
				  float cost, float shake, bool isPlop, int timeout);
	virtual ~CBFactoryTask();

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;
protected:
	virtual void Cancel() override;

private:
	virtual void FindBuildSite(CCircuitUnit* builder, const springai::AIFloat3& pos, float searchRadius) override;

	CCircuitDef* reprDef;
	bool isPlop;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FACTORYTASK_H_
