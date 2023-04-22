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
			  SResource cost, int timeout);
	CBGeoTask(ITaskManager* mgr);  // Load
	virtual ~CBGeoTask();

protected:
	virtual void Cancel() override;

	virtual bool Execute(CCircuitUnit* unit) override;

public:
	virtual void SetBuildPos(const springai::AIFloat3& pos) override;

private:
	virtual bool Load(std::istream& is) override;
	virtual void Save(std::ostream& os) const override;

	int spotId;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_GEOTASK_H_
