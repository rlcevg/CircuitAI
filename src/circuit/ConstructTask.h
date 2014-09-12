/*
 * ConstructTask.h
 *
 *  Created on: Sep 12, 2014
 *      Author: rlcevg
 */

#ifndef CONSTRUCTTASK_H_
#define CONSTRUCTTASK_H_

#include "UnitTask.h"

#include <list>

namespace circuit {

class IConstructTask: public IUnitTask {
public:
	IConstructTask(Priority priority, int difficulty, springai::AIFloat3& position, float radius, float metal, std::list<IConstructTask*>* owner);
	virtual ~IConstructTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	bool CompleteProgress(float metalStep);
	float GetMetalToSpend();

private:
	bool IsDistanceOk(springai::AIFloat3& pos);

	springai::AIFloat3 position;
	float sqradius;
	float metalToSpend;
	std::list<IConstructTask*>* owner;
};

} // namespace circuit

#endif // CONSTRUCTTASK_H_
