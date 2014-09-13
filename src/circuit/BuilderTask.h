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
	enum class TaskType: char {BUILD = 0, EXPAND, ENERGIZE, OVERDRIVE, DEFEND, ATTACK, ASSIST, TERRAFORM, DEFAULT = ASSIST};

public:
	CBuilderTask(Priority priority,
			springai::AIFloat3& position, std::list<IConstructTask*>& owner,
			TaskType type, int duration = 0);
	virtual ~CBuilderTask();

	void AssignTo(CCircuitUnit* unit);
	void RemoveAssignee(CCircuitUnit* unit);
	virtual bool CanAssignTo(CCircuitUnit* unit);

	TaskType GetType();
	int GetQuantity();
	int GetDuration();

	void SetBuildPos(springai::AIFloat3& pos);
	springai::AIFloat3& GetBuildPos();
	void SetTarget(CCircuitUnit* unit);
	CCircuitUnit* GetTarget();

private:
	TaskType type;
	int quantity;
	int duration;
	CCircuitUnit* target;
	springai::AIFloat3 buildPos;
};

} // namespace circuit

#endif // BUILDERTASK_H_
