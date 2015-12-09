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
				  CCircuitDef* buildDef, const springai::AIFloat3& position,
				  float cost, bool isShake, int timeout);
	virtual ~CBFactoryTask();

protected:
	virtual void Finish();
	virtual void Cancel();

private:
	virtual void FindBuildSite(CCircuitUnit* builder, const springai::AIFloat3& pos, float searchRadius) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FACTORYTASK_H_
