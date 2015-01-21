/*
 * ConstructTask.h
 *
 *  Created on: Sep 12, 2014
 *      Author: rlcevg
 */

#ifndef CONSTRUCTTASK_H_
#define CONSTRUCTTASK_H_

#include "task/UnitTask.h"

namespace springai {
	class UnitDef;
}

namespace circuit {

class IConstructTask: public IUnitTask {
public:
	enum class ConstructType: char {BUILDER, FACTORY};

protected:
	IConstructTask(Priority priority,
			springai::UnitDef* buildDef, const springai::AIFloat3& position, ConstructType conType);
public:
	virtual ~IConstructTask();

	ConstructType GetConstructType();
	const springai::AIFloat3& GetPos() const;
	springai::UnitDef* GetBuildDef();

protected:
	ConstructType conType;
	springai::AIFloat3 position;
	springai::UnitDef* buildDef;
};

} // namespace circuit

#endif // CONSTRUCTTASK_H_
