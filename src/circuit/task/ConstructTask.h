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

// TODO: Remove? It only used as common pos and buildDef container.
class IConstructTask: public IUnitTask {
protected:
	IConstructTask(Priority priority, Type type,
			springai::UnitDef* buildDef, const springai::AIFloat3& position);
public:
	virtual ~IConstructTask();

	const springai::AIFloat3& GetPos() const;
	springai::UnitDef* GetBuildDef();

protected:
	springai::AIFloat3 position;
	springai::UnitDef* buildDef;
};

} // namespace circuit

#endif // CONSTRUCTTASK_H_
