/*
 * AllyUnit.h
 *
 *  Created on: Nov 10, 2017
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ALLYUNIT_H_
#define SRC_CIRCUIT_UNIT_ALLYUNIT_H_

#include "unit/CoreUnit.h"

namespace circuit {

class CCircuitDef;
class IUnitTask;

class CAllyUnit: public ICoreUnit {
public:
	CAllyUnit(const CAllyUnit& that) = delete;
	CAllyUnit& operator=(const CAllyUnit&) = delete;
	CAllyUnit(Id unitId, springai::Unit* unit, CCircuitDef* cdef);
	virtual ~CAllyUnit();

	CCircuitDef* GetCircuitDef() const { return circuitDef; }
	IUnitTask* GetTask() const { return task; }
	const springai::AIFloat3& GetPos(int frame);

protected:
	CCircuitDef* circuitDef;
	IUnitTask* task;  // nullptr - ally or no handler (unable to control).

	int posFrame;
	springai::AIFloat3 position;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ALLYUNIT_H_
