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
			const springai::AIFloat3& position, ConstructType conType);
	virtual ~IConstructTask();

	ConstructType GetConstructType();
	const springai::AIFloat3& GetPos() const;

protected:
	ConstructType conType;
	springai::AIFloat3 position;
};

} // namespace circuit

#endif // CONSTRUCTTASK_H_
