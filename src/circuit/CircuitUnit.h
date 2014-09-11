/*
 * CircuitUnit.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef CIRCUITUNIT_H_
#define CIRCUITUNIT_H_

namespace springai {
	class Unit;
	class UnitDef;
}

namespace circuit {

#define CMD_PRIORITY	34220

class IModuleTask;

class CCircuitUnit {
public:
	CCircuitUnit(springai::Unit* unit, springai::UnitDef* def);
	virtual ~CCircuitUnit();

	springai::Unit* GetUnit();
	springai::UnitDef* GetDef();
	void SetTask(IModuleTask* task);
	IModuleTask* GetTask();

private:
	springai::Unit* unit;  // owner
	springai::UnitDef* def;
	IModuleTask* task;
};

} // namespace circuit

#endif // CIRCUITUNIT_H_
