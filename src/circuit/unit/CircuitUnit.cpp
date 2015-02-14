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

#include "Unit.h"

namespace circuit {

using namespace springai;

CCircuitUnit::CCircuitUnit(Unit* unit, UnitDef* def, CCircuitDef* circuitDef) :
		unit(unit),
		def(def),
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

Unit* CCircuitUnit::GetUnit()
{
	return unit;
}

UnitDef* CCircuitUnit::GetDef()
{
	return def;
}

CCircuitDef* CCircuitUnit::GetCircuitDef()
{
	return circuitDef;
}

void CCircuitUnit::SetTask(IUnitTask* task)
{
	this->task = task;
	taskFrame = manager->GetCircuit()->GetLastFrame();
}

IUnitTask* CCircuitUnit::GetTask()
{
	return task;
}

int CCircuitUnit::GetTaskFrame()
{
	return taskFrame;
}

void CCircuitUnit::SetManager(IUnitManager* mgr)
{
	manager = mgr;
}

IUnitManager* CCircuitUnit::GetManager()
{
	return manager;
}

void CCircuitUnit::SetArea(STerrainMapArea* area)
{
	this->area = area;
}

STerrainMapArea* CCircuitUnit::GetArea()
{
	return area;
}

} // namespace circuit
