/*
 * FactoryTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef FACTORYTASK_H_
#define FACTORYTASK_H_

#include "task/ConstructTask.h"

namespace circuit {

class CFactoryTask: public IConstructTask {
public:
	enum class TaskType: char {BUILDPOWER = 0, FIREPOWER, AA, CLOAK, DEFAULT = FIREPOWER};

public:
	CFactoryTask(Priority priority,
			springai::UnitDef* buildDef, const springai::AIFloat3& position,
			TaskType type, int quantity, float radius);
	virtual ~CFactoryTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	// TODO: Remove!
	virtual void RemoveAssignee(CCircuitUnit* unit);
	// TODO: Remove!
	virtual void MarkCompleted();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	TaskType GetType();

	void Progress();
	void Regress();
	bool IsDone();

private:
	TaskType type;
	int quantity;
	float sqradius;
};

} // namespace circuit

#endif // FACTORYTASK_H_
