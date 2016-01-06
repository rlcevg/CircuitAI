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

#include "AISCommands.h"
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
		, posFrame(-1)
		, moveFails(0)
		, failFrame(-1)
		, isForceExecute(false)
		, disarmParam(nullptr)
		, isDisarmed(false)
		, disarmFrame(-1)
		, ammoParam(nullptr)
		, isWeaponReady(true)
		, ammoFrame(-1)
		, isMorphing(false)
{
	WeaponMount* wpMnt;
	wpMnt = circuitDef->GetDGunMount();
	dgun = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
	wpMnt = circuitDef->GetShieldMount();
	shield = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
	wpMnt = circuitDef->GetWeaponMount();
	weapon = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
}

CCircuitUnit::~CCircuitUnit()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete unit;
	delete dgun;
	delete shield;
	delete disarmParam;
}

void CCircuitUnit::SetTask(IUnitTask* task)
{
	this->task = task;
	taskFrame = manager->GetCircuit()->GetLastFrame();
}

const AIFloat3& CCircuitUnit::GetPos(int frame)
{
	// NOTE: Ally units don't have manager, hence
	//       manager->GetCircuit()->GetLastFrame() will crash
	if (posFrame != frame) {
		posFrame = frame;
		position = unit->GetPos();
	}
	return position;
}

bool CCircuitUnit::IsMoveFailed(int frame)
{
	if (frame - failFrame >= FRAMES_PER_SEC * 3) {
		moveFails = 0;
	}
	failFrame = frame;
	return ++moveFails > TASK_RETRIES * 2;
}

bool CCircuitUnit::IsForceExecute()
{
	bool result = isForceExecute;
	isForceExecute = false;
	return result;
}

void CCircuitUnit::ManualFire(Unit* enemy, int timeOut)
{
	if (circuitDef->IsManualFire()) {
		unit->DGun(enemy, UNIT_COMMAND_OPTION_ALT_KEY, timeOut);
	} else {
		unit->ExecuteCustomCommand(CMD_ONECLICK_WEAPON, {}, UNIT_COMMAND_OPTION_ALT_KEY, timeOut);
	}
}

bool CCircuitUnit::IsDisarmed(int frame)
{
	if (disarmFrame != frame) {
		disarmFrame = frame;
		if (disarmParam == nullptr) {
			disarmParam = unit->GetUnitRulesParamByName("disarmed");
			if (disarmParam == nullptr) {
				return isDisarmed = false;
			}
		}
		isDisarmed = disarmParam->GetValueFloat() > .0f;
	}
	return isDisarmed;
}

bool CCircuitUnit::IsWeaponReady(int frame)
{
	if (ammoFrame != frame) {
		ammoFrame = frame;
		if (circuitDef->IsPlane()) {
			if (ammoParam == nullptr) {
				ammoParam = unit->GetUnitRulesParamByName("noammo");
				if (ammoParam == nullptr) {
					return isWeaponReady = true;
				}
			}
			isWeaponReady = ammoParam->GetValueFloat() < 1.0f;
		} else {
			isWeaponReady = (weapon == nullptr) ? false : weapon->GetReloadFrame() <= frame;
		}
	}
	return isWeaponReady;
}

bool CCircuitUnit::IsDGunReady(int frame)
{
	return dgun->GetReloadFrame() <= frame;
}

//bool CCircuitUnit::IsShieldCharged(float percent)
//{
//	return shield->GetShieldPower() > circuitDef->GetMaxShield() * percent;
//}

float CCircuitUnit::GetDPS()
{
	float dps = circuitDef->GetDPS();
	if (dps < 0.1f) {
		return .0f;
	}
	if (unit->IsParalyzed() || IsDisarmed(manager->GetCircuit()->GetLastFrame())) {
		return 1.0f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return dps;
}

void CCircuitUnit::Guard(CCircuitUnit* target, int timeout)
{
	unit->ExecuteCustomCommand(CMD_ORBIT, {(float)target->GetId(), 300.0f}, UNIT_COMMAND_OPTION_INTERNAL_ORDER, timeout);
//	unit->Guard(target->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, timeout);
//	unit->SetWantedMaxSpeed(MAX_SPEED);
}

void CCircuitUnit::Morph()
{
	isMorphing = true;
	unit->ExecuteCustomCommand(CMD_MORPH, {});
	unit->ExecuteCustomCommand(CMD_MISC_PRIORITY, {0.0f});
}

void CCircuitUnit::StopMorph()
{
	isMorphing = false;
	unit->ExecuteCustomCommand(CMD_MORPH_STOP, {});
	unit->ExecuteCustomCommand(CMD_MISC_PRIORITY, {1.0f});
}

} // namespace circuit
