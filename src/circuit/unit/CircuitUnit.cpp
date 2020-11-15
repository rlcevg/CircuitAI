/*
 * CircuitUnit.cpp
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "unit/action/DGunAction.h"
#include "unit/action/TravelAction.h"
#include "unit/enemy/EnemyUnit.h"
#include "setup/SetupManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"
#ifdef DEBUG_VIS
#include "task/UnitTask.h"
#include "Command.h"
#endif

#include "AISCommands.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Weapon.h"
#include "WrappWeaponMount.h"

namespace circuit {

using namespace springai;

CCircuitUnit::CCircuitUnit(Id unitId, Unit* unit, CCircuitDef* cdef)
		: CAllyUnit(unitId, unit, cdef)
		, taskFrame(-1)
		, manager(nullptr)
		, area(nullptr)
		, dgunAct(nullptr)
		, travelAct(nullptr)
		, moveFails(0)
		, failFrame(-1)
//		, damagedFrame(-1)
		, execFrame(-1)
		, disarmFrame(-1)
		, ammoFrame(-1)
		, priority(-1.f)
		, isDead(false)
		, isStuck(false)
		, isDisarmed(false)
		, isWeaponReady(true)
		, isMorphing(false)
		, isWaiting(false)
		, target(nullptr)
		, targetTile(-1)
{
	WeaponMount* wpMnt;
//	if (cdef->IsRoleComm()) {
//		dgun = nullptr;
//		for (int num = 1; num < 3; ++num) {
//			std::string str = utils::int_to_string(num, "comm_weapon_manual_%i");
//			if (unit->GetRulesParamFloat(str.c_str(), -1) <= 0.f) {
//				continue;
//			}
//			str = utils::int_to_string(num, "comm_weapon_num_%i");
//			int mntId = CWeaponDef::WeaponIdFromLua(int(unit->GetRulesParamFloat(str.c_str(), -1)));
//			if (mntId < 0) {
//				continue;
//			}
//			wpMnt = WrappWeaponMount::GetInstance(unit->GetSkirmishAIId(), cdef->GetId(), mntId);
//			if (wpMnt == nullptr) {
//				continue;
//			}
//			dgun = unit->GetWeapon(wpMnt);
//			delete wpMnt;
//			break;
//		}
//	} else {
		wpMnt = cdef->GetDGunMount();
		dgun = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
//	}
	wpMnt = cdef->GetWeaponMount();
	weapon = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
	wpMnt = cdef->GetShieldMount();
	shield = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
}

CCircuitUnit::~CCircuitUnit()
{
	delete dgun;
	delete weapon;
	delete shield;
}

void CCircuitUnit::SetTask(IUnitTask* task)
{
	this->task = task;
	SetTaskFrame(manager->GetCircuit()->GetLastFrame());
}

void CCircuitUnit::ClearAct()
{
	CActionList::Clear();
	dgunAct = nullptr;
	travelAct = nullptr;
}

void CCircuitUnit::PushDGunAct(CDGunAction* action)
{
	PushBack(action);
	dgunAct = action;
}

void CCircuitUnit::PushTravelAct(ITravelAction* action)
{
	PushBack(action);
	travelAct = action;
}

bool CCircuitUnit::IsMoveFailed(int frame)
{
	if (frame - failFrame >= FRAMES_PER_SEC * 3) {
		moveFails = 0;
	}
	failFrame = frame;
	isStuck = ++moveFails > TASK_RETRIES * 2;
	return isStuck;
}

bool CCircuitUnit::IsForceUpdate(int frame)
{
	if (execFrame > 0) {
		if (execFrame <= frame) {
			execFrame = -1;
			return true;
		}
	}
	return false;
}

void CCircuitUnit::ManualFire(CEnemyInfo* target, int timeout)
{
	TRY_UNIT(manager->GetCircuit(), this,
		if (circuitDef->HasDGun()) {
			if (target->GetUnit()->IsCloaked()) {  // los-cheat related
				unit->DGunPosition(target->GetPos(), UNIT_COMMAND_OPTION_ALT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY, timeout);
			} else {
				unit->DGun(target->GetUnit(), UNIT_COMMAND_OPTION_ALT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY, timeout);
			}
		} else {
			CmdMoveTo(target->GetPos() + target->GetVel() * FRAMES_PER_SEC * 2, UNIT_COMMAND_OPTION_ALT_KEY, timeout);
			CmdManualFire(UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
		}
	)
}

bool CCircuitUnit::IsDisarmed(int frame)
{
	if (disarmFrame != frame) {
		disarmFrame = frame;
		isDisarmed = unit->GetRulesParamFloat("disarmed", 0) > 0.f;
	}
	return isDisarmed;
}

bool CCircuitUnit::IsWeaponReady(int frame)
{
	if (ammoFrame != frame) {
		ammoFrame = frame;
		if (circuitDef->IsPlane()) {
			isWeaponReady = unit->GetRulesParamFloat("noammo", 0) < 1.f;
		} else {
			isWeaponReady = (weapon == nullptr) ? false : weapon->GetReloadFrame() <= frame;
		}
	}
	return isWeaponReady;
}

bool CCircuitUnit::IsDGunReady(int frame, float energy)
{
	return (dgun->GetReloadFrame() <= frame) && (circuitDef->GetDGunDef()->GetCostE() < energy + 1.f);
}

bool CCircuitUnit::IsShieldCharged(float percent)
{
	return shield->GetShieldPower() > circuitDef->GetMaxShield() * percent;
}

bool CCircuitUnit::IsJumpReady()
{
	return circuitDef->IsAbleToJump() && !(unit->GetRulesParamFloat("jumpReload", 1) < 1.f);
}

bool CCircuitUnit::IsJumping()
{
	return unit->GetRulesParamFloat("is_jumping", 0) > 0.f;
}

float CCircuitUnit::GetDamage()
{
	float dmg = circuitDef->GetPwrDamage();
	if (dmg < 1e-3f) {
		return 0.f;
	}
	if (unit->IsParalyzed() || IsDisarmed(manager->GetCircuit()->GetLastFrame())) {
		return 0.01f;
	}
	// TODO: Mind the slow down: dps * WeaponDef->GetReload / Weapon->GetReloadTime;
	return dmg;
}

float CCircuitUnit::GetShieldPower()
{
	if (shield != nullptr) {
		return shield->GetShieldPower();
	}
	return 0.f;
}

float CCircuitUnit::GetBuildSpeed()
{
	return circuitDef->GetBuildSpeed() * unit->GetRulesParamFloat("buildpower_mult", 1.f);
}

float CCircuitUnit::GetDGunRange()
{
	return dgun->GetRange() * unit->GetRulesParamFloat("comm_range_mult", 1.f);
}

float CCircuitUnit::GetHealthPercent()
{
	return unit->GetHealth() / unit->GetMaxHealth() - unit->GetCaptureProgress() * 16.f;
}

void CCircuitUnit::CmdRemove(std::vector<float>&& params, short options)
{
	unit->ExecuteCustomCommand(CMD_REMOVE, params, options);
}

void CCircuitUnit::CmdMoveTo(const AIFloat3& pos, short options, int timeout)
{
	unit->MoveTo(pos, options, timeout);
//	unit->ExecuteCustomCommand(CMD_RAW_MOVE, {pos.x, pos.y, pos.z}, options, timeout);
}

void CCircuitUnit::CmdJumpTo(const AIFloat3& pos, short options, int timeout)
{
//	unit->ExecuteCustomCommand(CMD_JUMP, {pos.x, pos.y, pos.z}, options, timeout);
}

void CCircuitUnit::CmdAttackGround(const AIFloat3& pos, short options, int timeout)
{
	unit->ExecuteCustomCommand(CMD_ATTACK_GROUND, {pos.x, pos.y, pos.z}, options, timeout);
}

void CCircuitUnit::CmdWantedSpeed(float speed)
{
//	unit->ExecuteCustomCommand(CMD_WANTED_SPEED, {speed});
}

void CCircuitUnit::CmdSetTarget(CEnemyInfo* enemy)
{
//	unit->ExecuteCustomCommand(CMD_UNIT_SET_TARGET, {(float)target->GetId()});
}

void CCircuitUnit::CmdCloak(bool state)
{
	unit->ExecuteCustomCommand(CMD_WANT_CLOAK, {state ? 1.f : 0.f});  // personal
//	unit->ExecuteCustomCommand(CMD_CLOAK_SHIELD, {state ? 1.f : 0.f});  // area
//	unit->Cloak(state);
}

void CCircuitUnit::CmdFireAtRadar(bool state)
{
//	unit->ExecuteCustomCommand(CMD_DONT_FIRE_AT_RADAR, {state ? 0.f : 1.f});
}

void CCircuitUnit::CmdFindPad(int timeout)
{
//	unit->ExecuteCustomCommand(CMD_FIND_PAD, {}, 0, timeout);
	unit->ExecuteCustomCommand(CMD_LAND_AT_AIRBASE, {}, 0, timeout);
}

void CCircuitUnit::CmdManualFire(short options, int timeout)
{
//	unit->ExecuteCustomCommand(CMD_ONECLICK_WEAPON, {}, options, timeout);
}

void CCircuitUnit::CmdPriority(float value)
{
//	unit->ExecuteCustomCommand(CMD_PRIORITY, {value});
}

void CCircuitUnit::CmdMiscPriority(float value)
{
//	unit->ExecuteCustomCommand(CMD_MISC_PRIORITY, {value});
}

void CCircuitUnit::CmdAirStrafe(float value)
{
//	unit->ExecuteCustomCommand(CMD_AIR_STRAFE, {value});
}

void CCircuitUnit::CmdBARPriority(float value)
{
	if (priority == value) {
		return;
	}
	priority = value;
	unit->ExecuteCustomCommand(CMD_BAR_PRIORITY, {value});
}

void CCircuitUnit::CmdTerraform(std::vector<float>&& params)
{
//	unit->ExecuteCustomCommand(CMD_TERRAFORM_INTERNAL, params);
}

void CCircuitUnit::CmdWait(bool state)
{
	if (!state) {
		auto commands = unit->GetCurrentCommands();
		for (springai::Command* cmd : commands) {
			if (cmd->GetId() == CMD_WAIT) {
				unit->Wait();
			}
			delete cmd;
		}
	} else {
		if (isWaiting != state) {
			unit->Wait();
		}
	}
	isWaiting = state;
}

void CCircuitUnit::RemoveWait()
{
	isWaiting = false;
	CmdRemove({CMD_WAIT}, UNIT_COMMAND_OPTION_ALT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
}

void CCircuitUnit::Attack(CEnemyInfo* enemy, int timeout)
{
	target = enemy;
	TRY_UNIT(manager->GetCircuit(), this,
		const AIFloat3& pos = enemy->GetPos();
		if (circuitDef->IsAttrMelee()) {
			if (IsJumpReady()) {
				CmdJumpTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
				unit->Attack(enemy->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
			} else {
				CmdMoveTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
				unit->Attack(enemy->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
			}
		} else {
			unit->Attack(enemy->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
		}
		unit->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);  // los-cheat related
		CmdWantedSpeed(NO_SPEED_LIMIT);
		CmdSetTarget(target);
	)
}

void CCircuitUnit::Attack(const AIFloat3& position, int timeout)
{
	const AIFloat3& pos = utils::get_radial_pos(position, SQUARE_SIZE * 8);
	TRY_UNIT(manager->GetCircuit(), this,
		if (circuitDef->IsAttrMelee()) {
			if (IsJumpReady()) {
				CmdJumpTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
				unit->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
			} else {
				CmdMoveTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
			}
		} else {
			unit->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
		}
		CmdWantedSpeed(NO_SPEED_LIMIT);
	)
}

void CCircuitUnit::Attack(const AIFloat3& pos, CEnemyInfo* enemy, int timeout)
{
	TRY_UNIT(manager->GetCircuit(), this,
		if (circuitDef->IsAttrMelee()) {
			if (IsJumpReady()) {
				CmdJumpTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
				unit->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
			} else {
				CmdMoveTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
			}
		} else {
			CmdMoveTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
		}
		unit->Attack(enemy->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
		unit->Fight(enemy->GetPos(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);  // los-cheat related
		CmdWantedSpeed(NO_SPEED_LIMIT);
		CmdSetTarget(target);
	)
}

void CCircuitUnit::Attack(const AIFloat3& position, CEnemyInfo* enemy, int tile, int timeout)
{
	target = enemy;
	targetTile = tile;
	Attack(position, enemy, timeout);
}

void CCircuitUnit::Guard(CCircuitUnit* target, int timeout)
{
	TRY_UNIT(manager->GetCircuit(), this,
//		unit->ExecuteCustomCommand(CMD_ORBIT, {(float)target->GetId(), 300.0f}, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
		unit->Guard(target->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
//		CmdWantedSpeed(NO_SPEED_LIMIT);
	)
}

void CCircuitUnit::Gather(const AIFloat3& groupPos, int timeout)
{
//	const AIFloat3& pos = utils::get_radial_pos(groupPos, SQUARE_SIZE * 8);
	TRY_UNIT(manager->GetCircuit(), this,
		CmdMoveTo(groupPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
		CmdWantedSpeed(NO_SPEED_LIMIT);
//		unit->PatrolTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
	)
}

void CCircuitUnit::Morph()
{
	isMorphing = true;
	TRY_UNIT(manager->GetCircuit(), this,
		unit->ExecuteCustomCommand(CMD_MORPH, {});
		CmdMiscPriority(1);
	)
}

void CCircuitUnit::StopMorph()
{
	isMorphing = false;
	TRY_UNIT(manager->GetCircuit(), this,
		unit->ExecuteCustomCommand(CMD_MORPH_STOP, {});
		CmdMiscPriority(1);
	)
}

bool CCircuitUnit::IsUpgradable()
{
	unsigned level = unit->GetRulesParamFloat("comm_level", 0.f);
	assert(manager != nullptr);
	return manager->GetCircuit()->GetSetupManager()->HasModules(circuitDef, level);
}

void CCircuitUnit::Upgrade()
{
	isMorphing = true;
	/*
	 * @see
	 * gui_chili_commander_upgrade.lua
	 * unit_morph.lua
	 * unit_commander_upgrade.lua
	 * dynamic_comm_defs.lua
	 *
	 * Level = params[1]
	 * Chassis = params[2]
	 * AlreadyCount = params[3]
	 * NewCount = params[4]
	 * OwnedModules = params[5..N]
	 * NewModules = params[N+1..M]
	 */

	float level = unit->GetRulesParamFloat("comm_level", 0.f);
	float chassis = unit->GetRulesParamFloat("comm_chassis", 0.f);
	float alreadyCount = unit->GetRulesParamFloat("comm_module_count", 0.f);

	assert(manager != nullptr);
	const std::vector<float>& newModules = manager->GetCircuit()->GetSetupManager()->GetModules(circuitDef, level);

	std::vector<float> upgrade;
	upgrade.push_back(level);
	upgrade.push_back(chassis);
	upgrade.push_back(alreadyCount);
	upgrade.push_back(newModules.size());

	for (int i = 1; i <= alreadyCount; ++i) {
		std::string modId = utils::int_to_string(i, "comm_module_%i");
		float value = unit->GetRulesParamFloat(modId.c_str(), -1.f);
		if (value != -1.f) {
			upgrade.push_back(value);
		}
	}

	upgrade.insert(upgrade.end(), newModules.begin(), newModules.end());

	TRY_UNIT(manager->GetCircuit(), this,
		unit->ExecuteCustomCommand(CMD_MORPH_UPGRADE_INTERNAL, upgrade);
		CmdMiscPriority(1);
	)
}

void CCircuitUnit::StopUpgrade()
{
	isMorphing = false;
	TRY_UNIT(manager->GetCircuit(), this,
		unit->ExecuteCustomCommand(CMD_UPGRADE_STOP, {});
		CmdMiscPriority(1);
	)
}

#ifdef DEBUG_VIS
void CCircuitUnit::Log()
{
	if (task != nullptr) {
		task->Log();
	}
	CCircuitAI* circuit = manager->GetCircuit();
	circuit->LOG("unit: %i | id: %i | %s", this, id, circuitDef->GetDef()->GetName());
	auto commands = unit->GetCurrentCommands();
	for (springai::Command* c : commands) {
		circuit->LOG("command: %i | type: %i | id: %i", c->GetCommandId(), c->GetType(), c->GetId());
	}
	utils::free_clear(commands);
}
#endif

} // namespace circuit
