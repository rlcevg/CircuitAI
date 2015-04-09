/*
 * CircuitUnit.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CCircuitUnit::CCircuitUnit(Unit* unit, CCircuitDef* circuitDef) :
		id(unit->GetUnitId()),
		unit(unit),
		circuitDef(circuitDef),
		task(nullptr),
		taskFrame(-1),
		manager(nullptr),
		area(nullptr)
{
}

CCircuitUnit::~CCircuitUnit()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete unit;
}

CCircuitUnit::Id CCircuitUnit::GetId() const
{
	return id;
}

Unit* CCircuitUnit::GetUnit() const
{
	return unit;
}

CCircuitDef* CCircuitUnit::GetCircuitDef() const
{
	return circuitDef;
}

void CCircuitUnit::SetTask(IUnitTask* task)
{
	this->task = task;
	taskFrame = manager->GetCircuit()->GetLastFrame();
}

IUnitTask* CCircuitUnit::GetTask() const
{
	return task;
}

int CCircuitUnit::GetTaskFrame() const
{
	return taskFrame;
}

void CCircuitUnit::SetManager(IUnitManager* mgr)
{
	manager = mgr;
}

IUnitManager* CCircuitUnit::GetManager() const
{
	return manager;
}

void CCircuitUnit::SetArea(STerrainMapArea* area)
{
	this->area = area;
}

STerrainMapArea* CCircuitUnit::GetArea() const
{
	return area;
}

} // namespace circuit
