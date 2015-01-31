/*
 * CircuitUnit.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_
#define SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_

#include "util/ActionList.h"

namespace springai {
	class Unit;
	class UnitDef;
}

namespace circuit {

#define CMD_PRIORITY			34220
#define CMD_TERRAFORM_INTERNAL	39801
//#define CMD_RETREAT_ZONE		10001
//#define CMD_SETHAVEN			CMD_RETREAT_ZONE
#define CMD_RETREAT				34223

class CCircuitDef;
class IUnitTask;
class IUnitManager;

class CCircuitUnit: public CActionList {
public:
	CCircuitUnit(springai::Unit* unit, springai::UnitDef* def, CCircuitDef* circuitDef);
	virtual ~CCircuitUnit();

	springai::Unit* GetUnit();
	springai::UnitDef* GetDef();
	CCircuitDef* GetCircuitDef();

	void SetTask(IUnitTask* task);
	IUnitTask* GetTask();
	int GetTaskFrame();

	void SetManager(IUnitManager* mgr);
	IUnitManager* GetManager();

private:
	springai::Unit* unit;  // owner
	springai::UnitDef* def;
	CCircuitDef* circuitDef;
	IUnitTask* task;
	int taskFrame;
	IUnitManager* manager;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_
