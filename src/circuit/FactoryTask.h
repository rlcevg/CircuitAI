/*
 * FactoryTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef FACTORYTASK_H_
#define FACTORYTASK_H_

#include "ConstructTask.h"

namespace circuit {

class CFactoryTask: public IConstructTask {
public:
	enum class TaskType: char {BUILDPOWER = 0, FIREPOWER, AA, CLOAK, DEFAULT = FIREPOWER};

public:
	CFactoryTask(Priority priority,
			const springai::AIFloat3& position,
			TaskType type, int quantity, float radius);
	virtual ~CFactoryTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);

	TaskType GetType();

	void Progress();
	void Regress();
	bool IsDone();

private:
	bool IsDistanceOk(const springai::AIFloat3& pos);

	TaskType type;
	int quantity;
	float sqradius;
};

} // namespace circuit

#endif // FACTORYTASK_H_
