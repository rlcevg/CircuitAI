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

#include "UnitRulesParam.h"
#include "Weapon.h"

namespace circuit {

using namespace springai;

CCircuitUnit::CCircuitUnit(Unit* unit, CCircuitDef* cdef)
		: id(unit->GetUnitId())
		, unit(unit)
		, circuitDef(cdef)
		, task(nullptr)
		, taskFrame(-1)
		, manager(nullptr)
		, area(nullptr)
		, disarmParam(nullptr)
		, moveFails(0)
		, failFrame(-1)
{
	WeaponMount* wpMnt = circuitDef->GetDGunMount();
	dgun = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
}

CCircuitUnit::~CCircuitUnit()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete unit, dgun, disarmParam;
}

void CCircuitUnit::SetTask(IUnitTask* task)
{
	this->task = task;
	taskFrame = manager->GetCircuit()->GetLastFrame();
}

bool CCircuitUnit::IsMoveFailed(int frame)
{
	if (frame - failFrame >= FRAMES_PER_SEC * 3) {
		moveFails = 0;
	}
	failFrame = frame;
	return ++moveFails > TASK_RETRIES * 2;
}

bool CCircuitUnit::IsDisarmed()
{
	if (disarmParam == nullptr) {
		disarmParam = unit->GetUnitRulesParamByName("disarmed");
		if (disarmParam == nullptr) {
			return false;
		}
	}
	return disarmParam->GetValueFloat() > .0f;
}

float CCircuitUnit::GetDPS()
{
	float dps = circuitDef->GetDPS();
	if (dps < 0.1f) {
		return .0f;
	}
	if (unit->IsParalyzed() || IsDisarmed()) {
		return 1.0f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return dps;
}

} // namespace circuit
