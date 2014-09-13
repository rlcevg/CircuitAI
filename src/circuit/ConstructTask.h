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
	IConstructTask(Priority priority, int quantity, springai::AIFloat3& position, std::list<IConstructTask*>& owner);
	virtual ~IConstructTask();

	void Progress();
	void Regress();
	bool IsDone();
	void MarkCompleted();

public:
	int quantity;
	springai::AIFloat3 position;
	std::list<IConstructTask*>* owner;
};

} // namespace circuit

#endif // CONSTRUCTTASK_H_
