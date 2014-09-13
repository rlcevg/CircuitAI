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
	CBuilderTask(Priority priority, int quantity, springai::AIFloat3& position, std::list<IConstructTask*>& owner, TaskType type, float time);
	virtual ~CBuilderTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	TaskType GetType();

private:
	virtual bool IsDistanceOk(springai::AIFloat3& pos, float speed);

	TaskType type;
	float time;  // seconds
};

} // namespace circuit

#endif // BUILDERTASK_H_
