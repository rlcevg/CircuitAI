/*
 * BuilderTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef BUILDERTASK_H_
#define BUILDERTASK_H_

#include "ConstructTask.h"

#define MIN_BUILD_SEC	8
#define MAX_TRAVEL_SEC	60

namespace circuit {

class CBuilderTask: public IConstructTask {
public:
	enum class TaskType: int {
		FACTORY = 0, NANO,
		EXPAND, STORE,
		SOLAR, FUSION, SINGU, PYLON,
		DEFENDER, LOTUS, DDM, ANNI,
		RADAR, TERRAFORM, ASSIST, RECLAIM, TASKS_COUNT, DEFAULT = DEFENDER
	};

public:
	CBuilderTask(Priority priority,
			springai::UnitDef* buildDef, const springai::AIFloat3& position,
			TaskType type, float cost, int timeout = 0);
	virtual ~CBuilderTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	TaskType GetType();
	float GetBuildPower();
	float GetCost();
	int GetTimeout();

	void SetBuildPos(const springai::AIFloat3& pos);
	const springai::AIFloat3& GetBuildPos() const;
	void SetTarget(CCircuitUnit* unit);
	CCircuitUnit* GetTarget();

private:
	TaskType type;
	float buildPower;
	float cost;
	int timeout;
	CCircuitUnit* target;
	springai::AIFloat3 buildPos;
};

} // namespace circuit

#endif // BUILDERTASK_H_
