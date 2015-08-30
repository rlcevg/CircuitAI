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

CCircuitUnit::CCircuitUnit(Unit* unit, CCircuitDef* circuitDef)
		: id(unit->GetUnitId())
		, unit(unit)
		, dgun(nullptr)
		, task(nullptr)
		, taskFrame(-1)
		, manager(nullptr)
		, area(nullptr)
		, disarmParam(nullptr)
		, moveFails(0)
		, unitFrame({-1})
{
	SetCircuitDef(circuitDef);
}

CCircuitUnit::~CCircuitUnit()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete unit, dgun, disarmParam;
}

void CCircuitUnit::SetCircuitDef(CCircuitDef* cdef)
{
	circuitDef = cdef;
	delete dgun;
	if (cdef == nullptr) {
		dgun = nullptr;
	} else {
		WeaponMount* wpMnt = circuitDef->GetDGunMount();
		dgun = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
	}
}

void CCircuitUnit::SetTask(IUnitTask* task)
{
	this->task = task;
	taskFrame = manager->GetCircuit()->GetLastFrame();
}

bool CCircuitUnit::IsMoveFailed(int frame)
{
	if (frame - unitFrame.failFrame >= FRAMES_PER_SEC * 3) {
		moveFails = 0;
	}
	unitFrame.failFrame = frame;
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
	if (circuitDef == nullptr) {
		return 100.0f;
	}
	float dps = circuitDef->GetDPS();
	if ((dps < 0.1f) || unit->IsParalyzed() || IsDisarmed()) {
		return 0.0f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return dps;
}

} // namespace circuit
