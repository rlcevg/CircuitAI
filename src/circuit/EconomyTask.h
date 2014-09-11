/*
 * EconomyTask.h
 *
 *  Created on: Sep 10, 2014
 *      Author: rlcevg
 */

#ifndef ECONOMYTASK_H_
#define ECONOMYTASK_H_

#include "ModuleTask.h"

namespace circuit {

class CCircuitUnit;

class CEconomyTask: public virtual IModuleTask {
public:
	enum class TaskType: char {BUILD = 0, ENERGY, OVERDRIVE, DEFEND, ATTACK, TERRAFORM, DEFAULT = 0};

public:
	CEconomyTask(TaskType type);
	virtual ~CEconomyTask();

	virtual bool Execute(CCircuitUnit* unit);

private:
	TaskType type;
};

} // namespace circuit

#endif // ECONOMYTASK_H_
