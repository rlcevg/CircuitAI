/*
 * UnitTask.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef UNITTASK_H_
#define UNITTASK_H_

#include "AIFloat3.h"

#include <unordered_set>
#include "utils.h"
#include "CircuitAI.h"
#include "Log.h"
#include "Unit.h"

namespace circuit {

class CCircuitUnit;

class IUnitTask {
public:
	enum class Priority: char {LOW = 0, /*NORMAL,*/ HIGH};

public:
	IUnitTask(Priority priority);
	virtual ~IUnitTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) = 0;
	virtual void AssignTo(CCircuitUnit* unit, CCircuitAI* circuit);
	virtual void RemoveAssignee(CCircuitUnit* unit);
	void MarkCompleted(CCircuitAI* circuit=nullptr);

	std::unordered_set<CCircuitUnit*>& GetAssignees();

protected:
	std::unordered_set<CCircuitUnit*> units;
	Priority priority;
};

} // namespace circuit

#endif // UNITTASK_H_
