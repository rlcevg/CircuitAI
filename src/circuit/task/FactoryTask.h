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
	enum class FacType: char {BUILDPOWER = 0, FIREPOWER, AA, CLOAK, DEFAULT = FIREPOWER};

public:
	CFactoryTask(Priority priority,
			springai::UnitDef* buildDef, const springai::AIFloat3& position,
			FacType type, int quantity, float radius);
	virtual ~CFactoryTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);

	virtual void Update(CCircuitAI* circuit);

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	FacType GetFacType();

	void Progress();
	void Regress();
	bool IsDone();

private:
	FacType facType;
	int quantity;
	float sqradius;
};

} // namespace circuit

#endif // FACTORYTASK_H_
