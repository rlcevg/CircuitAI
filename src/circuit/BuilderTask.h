/*
 * BuilderTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef BUILDERTASK_H_
#define BUILDERTASK_H_

#include "ConstructTask.h"

namespace circuit {

class CBuilderTask: public IConstructTask {
public:
	enum class TaskType: char {
		FACTORY = 0, NANO,
		EXPAND,
		SOLAR, FUSION, SINGU, PYLON,
		DEFENDER, LOTUS, DDM, ANNI,
		RADAR, TERRAFORM, ASSIST, DEFAULT = DEFENDER
	};

public:
	CBuilderTask(Priority priority,
			const springai::AIFloat3& position,
			TaskType type, int timeout = 0);
	virtual ~CBuilderTask();

	void AssignTo(CCircuitUnit* unit);
	void RemoveAssignee(CCircuitUnit* unit);
	virtual bool CanAssignTo(CCircuitUnit* unit);

	TaskType GetType();
	int GetQuantity();
	int GetTimeout();

	void SetBuildPos(const springai::AIFloat3& pos);
	springai::AIFloat3& GetBuildPos();
	void SetTarget(CCircuitUnit* unit);
	CCircuitUnit* GetTarget();

private:
	TaskType type;
	int quantity;
	int timeout;
	CCircuitUnit* target;
	springai::AIFloat3 buildPos;
};

} // namespace circuit

#endif // BUILDERTASK_H_
