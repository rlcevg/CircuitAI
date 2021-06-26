/*
 * GeoTask.h
 *
 *  Created on: Jun 25, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_GEOTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_GEOTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBGeoTask: public IBuilderTask {
public:
	CBGeoTask(ITaskManager* mgr, Priority priority,
			  CCircuitDef* buildDef, int spotId, const springai::AIFloat3& position,
			  float cost, int timeout);
	virtual ~CBGeoTask();

protected:
	virtual void Cancel() override;

	virtual void Execute(CCircuitUnit* unit) override;

public:
	virtual void SetBuildPos(const springai::AIFloat3& pos) override;

private:
	int spotId;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_GEOTASK_H_
