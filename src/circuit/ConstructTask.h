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
	enum class ConstructType: char {BUILDER = 0, FACTORY};

public:
	IConstructTask(Priority priority,
			springai::AIFloat3& position, std::list<IConstructTask*>& owner, ConstructType conType);
	virtual ~IConstructTask();

	void MarkCompleted();

	ConstructType GetConstructType();
	springai::AIFloat3& GetPos();

protected:
	ConstructType conType;
	springai::AIFloat3 position;
	std::list<IConstructTask*>* owner;
};

} // namespace circuit

#endif // CONSTRUCTTASK_H_
