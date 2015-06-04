/*
 * CircuitUnit.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_
#define SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_

#include "util/ActionList.h"

#include "Unit.h"

namespace circuit {

#define CMD_PRIORITY			34220
#define CMD_TERRAFORM_INTERNAL	39801
//#define CMD_RETREAT_ZONE		10001
//#define CMD_SETHAVEN			CMD_RETREAT_ZONE
#define CMD_RETREAT				34223

class CCircuitDef;
class IUnitTask;
class IUnitManager;
struct STerrainMapArea;

class CCircuitUnit: public CActionList {
public:
	using Id = int;

	CCircuitUnit(springai::Unit* unit, CCircuitDef* circuitDef);
	virtual ~CCircuitUnit();

	Id GetId() const { return id; }
	springai::Unit* GetUnit() const { return unit; }
	CCircuitDef* GetCircuitDef() const { return circuitDef; }

	void SetTask(IUnitTask* task);
	IUnitTask* GetTask() const { return task; }
	int GetTaskFrame() const { return taskFrame; }

	void SetManager(IUnitManager* mgr) { manager = mgr; }
	IUnitManager* GetManager() const { return manager; }

	void SetArea(STerrainMapArea* area) { this->area = area; }
	STerrainMapArea* GetArea() const { return area; }

	void MoveFailed() { ++moveFails; }
	bool IsMoveFailed() const { return moveFails > 100; }

	void ManualFire(const springai::AIFloat3& pos, int frame);
	void ManualFire(springai::Unit* enemy, int frame);
	int GetDGunFrame() const { return dgunFrame; }

	bool operator==(const CCircuitUnit& rhs) { return id == rhs.id; }
	bool operator!=(const CCircuitUnit& rhs) { return id != rhs.id; }

private:
	Id id;
	springai::Unit* unit;  // owner
	CCircuitDef* circuitDef;  // TODO: Replace with CCircuitDef::Id?
	IUnitTask* task;
	int taskFrame;
	IUnitManager* manager;
	STerrainMapArea* area;  // = nullptr if a unit flies

	int moveFails;

	int dgunFrame;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_
