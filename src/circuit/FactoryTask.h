/*
 * FactoryTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef FACTORYTASK_H_
#define FACTORYTASK_H_

#include "ConstructTask.h"

namespace circuit {

class CFactoryTask: public IConstructTask {
public:
	enum class TaskType: char {BUILDPOWER = 0, FIREPOWER, AA, CLOAK, DEFAULT = BUILDPOWER};

public:
	CFactoryTask(Priority priority, int quantity, springai::AIFloat3& position, std::list<IConstructTask*>& owner, TaskType type, float radius);
	virtual ~CFactoryTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	TaskType GetType();

private:
	bool IsDistanceOk(springai::AIFloat3& pos);

	TaskType type;
	float sqradius;
};

} // namespace circuit

#endif // FACTORYTASK_H_
