/*
 * CircuitUnit.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "unit/action/WaitAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CCircuitUnit::CCircuitUnit(Unit* unit, CCircuitDef* circuitDef) :
		id(unit->GetUnitId()),
		unit(unit),
		circuitDef(circuitDef),
		task(nullptr),
		taskFrame(-1),
		manager(nullptr),
		area(nullptr),
		dgunFrame(-1)
{
	PushBack(new CWaitAction(this));
}

CCircuitUnit::~CCircuitUnit()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete unit;
}

void CCircuitUnit::SetTask(IUnitTask* task)
{
	this->task = task;
	taskFrame = manager->GetCircuit()->GetLastFrame();
}

void CCircuitUnit::ManualFire(const AIFloat3& pos, int frame)
{
	dgunFrame = frame;
	unit->DGunPosition(pos, UNIT_COMMAND_OPTION_ALT_KEY, FRAMES_PER_SEC * 5);
}

void CCircuitUnit::ManualFire(Unit* enemy, int frame)
{
	dgunFrame = frame;
	unit->DGun(enemy, UNIT_COMMAND_OPTION_ALT_KEY, FRAMES_PER_SEC * 5);
}

} // namespace circuit
