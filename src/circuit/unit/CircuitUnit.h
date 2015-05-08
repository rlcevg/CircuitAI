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

	inline Id GetId() const { return id; }
	inline springai::Unit* GetUnit() const { return unit; }
	inline CCircuitDef* GetCircuitDef() const { return circuitDef; }

	void SetTask(IUnitTask* task);
	inline IUnitTask* GetTask() const { return task; }
	inline int GetTaskFrame() const { return taskFrame; }

	inline void SetManager(IUnitManager* mgr) { manager = mgr; }
	inline IUnitManager* GetManager() const { return manager; }

	inline void SetArea(STerrainMapArea* area) { this->area = area; }
	inline STerrainMapArea* GetArea() const { return area; }

	inline bool operator==(const CCircuitUnit& rhs);
	inline bool operator!=(const CCircuitUnit& rhs);

private:
	Id id;
	springai::Unit* unit;  // owner
	CCircuitDef* circuitDef;  // TODO: Replace with CCircuitDef::Id?
	IUnitTask* task;
	int taskFrame;
	IUnitManager* manager;
	STerrainMapArea* area;  // = nullptr if a unit flies
};

inline bool CCircuitUnit::operator==(const CCircuitUnit& rhs)
{
	return id == rhs.id;
}

inline bool CCircuitUnit::operator!=(const CCircuitUnit& rhs)
{
	return id != rhs.id;
}

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_
