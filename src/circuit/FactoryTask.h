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
	CFactoryTask(Priority priority, int difficulty, springai::AIFloat3& position, float radius, float metal, TaskType type, std::list<IConstructTask*>* owner);
	virtual ~CFactoryTask();

	TaskType GetType();

private:
	TaskType type;
};

} // namespace circuit

#endif // FACTORYTASK_H_
