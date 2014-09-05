/*
 * UnitTask.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef UNITTASK_H_
#define UNITTASK_H_

namespace circuit {

class CCircuitUnit;

class CUnitTask {
public:
	CUnitTask();
	virtual ~CUnitTask();

private:
	CCircuitUnit* unit;

};

} // namespace circuit

#endif // UNITTASK_H_
