/*
 * UnitTask.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef UNITTASK_H_
#define UNITTASK_H_

#include "AIFloat3.h"

namespace circuit {

class CCircuitUnit;

class CUnitTask {
public:
	enum class TaskType: char {BUILD = 0, DEFEND, ATTACK, TERRAFORM};

public:
	CUnitTask();
	virtual ~CUnitTask();

private:
	CCircuitUnit* unit;

	/*
	 * task attributes
	 */
	TaskType type;
	springai::AIFloat3 posFrom;
	springai::AIFloat3 posTo;
	float radius;
	int duration;  // in frames
	CCircuitUnit* enemy;
};

} // namespace circuit

#endif // UNITTASK_H_
