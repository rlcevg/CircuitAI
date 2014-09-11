/*
 * ModuleTask.h
 *
 *  Created on: Sep 10, 2014
 *      Author: rlcevg
 */

#ifndef MODULETASK_H_
#define MODULETASK_H_

#include <list>

namespace circuit {

class CUnitTask;
class CCircuitUnit;

class IModuleTask {
public:
	enum class Priority: char {LOW = 0, NORMAL, HIGH};

public:
	IModuleTask();
	virtual ~IModuleTask();

	virtual bool Execute(CCircuitUnit* unit) = 0;

protected:
	std::list<CUnitTask*> subtasks;  // owner
	Priority priority;
	int difficulty;
};

} // namespace circuit

#endif // MODULETASK_H_
