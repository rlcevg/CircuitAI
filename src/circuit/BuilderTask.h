/*
 * BuilderTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef BUILDERTASK_H_
#define BUILDERTASK_H_

#include "UnitTask.h"

namespace circuit {

class CBuilderTask: public IUnitTask {
public:
	enum class TaskType: char {BUILD = 0, EXPAND, ENERGIZE, OVERDRIVE, DEFEND, ATTACK, ASSIST, TERRAFORM, DEFAULT = BUILD};

public:
	CBuilderTask(Priority priority, int difficulty, TaskType type, springai::AIFloat3& position, float radius);
	virtual ~CBuilderTask();

	virtual bool AssignTo(CCircuitUnit* unit);
	virtual void MarkCompleted();
	TaskType GetType();

private:
	TaskType type;
	springai::AIFloat3 position;
	float sqradius;

	bool IsDistanceOk(springai::AIFloat3& pos);
};

} // namespace circuit

#endif // BUILDERTASK_H_
