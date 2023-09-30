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
#include "terrain/TerrainManager.h"  // Only for CorrectPosition
#include "CircuitAI.h"
#include "util/Utils.h"
#ifdef DEBUG_VIS
#include "task/UnitTask.h"
#endif

#include "AISCommands.h"
#include "Sim/Units/CommandAI/Command.h"
#include "WrappCurrentCommand.h"
#include "Weapon.h"
#include "WrappWeaponMount.h"

namespace circuit {

using namespace springai;

CCircuitUnit::CCircuitUnit(CCircuitAI* circuit, Id unitId, Unit* unit, CCircuitDef* cdef)
		: CAllyUnit(unitId, unit, cdef)
		, taskFrame(-1)
		, taskState(ETaskState::NONE)
		, manager(nullptr)
		, area(nullptr)
		, dgunAct(nullptr)
		, travelAct(nullptr)
		, moveFails(0)
		, failFrame(-1)
		, damagedFrame(-1)
		, execFrame(-1)
		, disarmFrame(-1)
		, ammoFrame(-1)
		, priority(-1.f)
		, isDead(false)
		, isStuck(false)
		, isDisarmed(false)
		, isWeaponReady(true)
		, isMorphing(false)
		, isSelfD(false)
		, target(nullptr)
		, targetTile(-1)
		, attr(cdef->GetAttributes())
{
	command = springai::WrappCurrentCommand::GetInstance(unit->GetSkirmishAIId(), id, 0);

	WeaponMount* wpMnt;
//	if (cdef->IsRoleComm()) {
//		dgun = nullptr;
//		dgunDef = nullptr;
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
//			WeaponDef* wd = dgun->GetDef();
//			dgunDef = circuit->GetWeaponDef(wd->GetWeaponDefId());
//			delete wd;
//			delete wpMnt;
//			break;
//		}
//	} else {
		wpMnt = cdef->GetDGunMount();
		dgun = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
		dgunDef = cdef->GetDGunDef();
//	}
	wpMnt = cdef->GetWeaponMount();
	weapon = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
	wpMnt = cdef->GetShieldMount();
	shield = (wpMnt == nullptr) ? nullptr : unit->GetWeapon(wpMnt);
}

CCircuitUnit::~CCircuitUnit()
{
	delete command;
	delete dgun;
	delete weapon;
	delete shield;
}

void CCircuitUnit::SetTask(IUnitTask* task)
{
	this->task = task;
	SetTaskFrame(manager->GetCircuit()->GetLastFrame());
	taskState = ETaskState::NONE;
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

void CCircuitUnit::ForceUpdate(int frame)
{
	if (execFrame < 0) {
		execFrame = frame;
	}
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
			AIFloat3 leadPos = target->GetPos() + target->GetVel() * FRAMES_PER_SEC * 2;
			CTerrainManager::CorrectPosition(leadPos);
			CmdMoveTo(leadPos, UNIT_COMMAND_OPTION_ALT_KEY, timeout);
			CmdManualFire(UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
		}
	)
}

bool CCircuitUnit::IsDGunHigh() const
{
	return dgunDef->IsHighTrajectory();
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
	return (dgun->GetReloadFrame() <= frame) && (dgunDef->GetCostE() < energy)
		&& (!dgunDef->IsStockpile() || (unit->GetStockpile() > 0));
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

bool CCircuitUnit::IsInvisible()
{
	// FIXME: lua can Spring.SetUnitStealth()
	return circuitDef->IsStealth() && unit->IsCloaked();
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

float CCircuitUnit::GetWorkerTime()
{
	return circuitDef->GetWorkerTime() * unit->GetRulesParamFloat("buildpower_mult", 1.f);
}

float CCircuitUnit::GetDGunRange()
{
	return dgun->GetRange() * unit->GetRulesParamFloat("comm_range_mult", 1.f);
}

float CCircuitUnit::GetHealthPercent()
{
	return unit->GetHealth() / unit->GetMaxHealth() - unit->GetCaptureProgress() * 16.f;
}

/*
 * UNIT_COMMAND_OPTION_ALT_KEY - remove by commandId, otherwise - by tag
 * UNIT_COMMAND_OPTION_CONTROL_KEY - remove from factory queue
 */
void CCircuitUnit::CmdRemove(std::vector<float>&& params, short options)
{
	unit->ExecuteCustomCommand(CMD_REMOVE, params, options);
}

void CCircuitUnit::CmdMoveTo(const AIFloat3& pos, short options, int timeout)
{
	assert(utils::is_in_map(pos));
	unit->MoveTo(pos, options, timeout);
//	unit->ExecuteCustomCommand(CMD_RAW_MOVE, {pos.x, pos.y, pos.z}, options, timeout);
}

void CCircuitUnit::CmdJumpTo(const AIFloat3& pos, short options, int timeout)
{
//	assert(utils::is_in_map(pos));
//	unit->ExecuteCustomCommand(CMD_JUMP, {pos.x, pos.y, pos.z}, options, timeout);
}

void CCircuitUnit::CmdFightTo(const AIFloat3& pos, short options, int timeout)
{
	assert(utils::is_in_map(pos));
	unit->Fight(pos, options, timeout);
}

void CCircuitUnit::CmdPatrolTo(const AIFloat3& pos, short options, int timeout)
{
	assert(utils::is_in_map(pos));
	unit->PatrolTo(pos, options, timeout);

}

void CCircuitUnit::CmdAttackGround(const AIFloat3& pos, short options, int timeout)
{
	assert(utils::is_in_map(pos));
	unit->ExecuteCustomCommand(CMD_ATTACK_GROUND, {pos.x, pos.y, pos.z}, options, timeout);
}

void CCircuitUnit::CmdWantedSpeed(float speed)
{
//	unit->SetWantedMaxSpeed(speed / FRAMES_PER_SEC, true);
//	unit->SetWantedMaxSpeed(0.5f, true);
//	unit->ExecuteCustomCommand(CMD_WANTED_SPEED, {speed});
}

void CCircuitUnit::CmdStop(short options, int timeout)
{
	unit->Stop(options, timeout);
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

void CCircuitUnit::CmdSelfD(bool state)
{
	if (isSelfD != state) {
		unit->SelfDestruct();
		isSelfD = state;
	}
}

void CCircuitUnit::CmdWait(bool state)
{
	if (state != (command->GetId() == CMD_WAIT)) {
		unit->Wait();
	}
}

void CCircuitUnit::RemoveWait()
{
	CmdRemove({CMD_WAIT}, UNIT_COMMAND_OPTION_ALT_KEY | UNIT_COMMAND_OPTION_CONTROL_KEY);
}

bool CCircuitUnit::IsWaiting() const
{
	return command->GetId() == CMD_WAIT;
}

void CCircuitUnit::CmdRepair(CAllyUnit* target, short options, int timeout)
{
	unit->Repair(target->GetUnit(), options, timeout);
	taskState = ETaskState::EXECUTE;
}

void CCircuitUnit::CmdBuild(CCircuitDef* buildDef, const AIFloat3& buildPos, int facing, short options, int timeout)
{
	unit->Build(buildDef->GetDef(), buildPos, facing, options, timeout);
	taskState = ETaskState::EXECUTE;
}

void CCircuitUnit::CmdReclaimEnemy(CEnemyInfo* enemy, short options, int timeout)
{
	unit->ReclaimUnit(enemy->GetUnit(), options, timeout);
}

void CCircuitUnit::CmdReclaimUnit(CAllyUnit* toReclaim, short options, int timeout)
{
	unit->ReclaimUnit(toReclaim->GetUnit(), options, timeout);
	taskState = ETaskState::EXECUTE;
}

void CCircuitUnit::CmdReclaimInArea(const AIFloat3& pos, float radius, short options, int timeout)
{
	unit->ReclaimInArea(pos, radius, options, timeout);
}

void CCircuitUnit::CmdResurrectInArea(const AIFloat3& pos, float radius, short options, int timeout)
{
	unit->ResurrectInArea(pos, radius, options, timeout);
}

void CCircuitUnit::Attack(CEnemyInfo* enemy, bool isGround, int timeout)
{
	target = enemy;
	TRY_UNIT(manager->GetCircuit(), this,
		const AIFloat3& pos = enemy->GetPos();
		if (circuitDef->IsAttrMelee()) {
			if (IsJumpReady()) {
				CmdJumpTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
				if (isGround) {  // los-cheat related
					CmdAttackGround(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
				} else {
					unit->Attack(enemy->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
				}
			} else {
				CmdMoveTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
				if (isGround) {  // los-cheat related
					CmdAttackGround(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
				} else {
					unit->Attack(enemy->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
				}
			}
		} else {
			if (isGround) {  // los-cheat related
				CmdAttackGround(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
			} else {
				unit->Attack(enemy->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
			}
		}
		CmdFightTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);  // los-cheat related
		CmdWantedSpeed(NO_SPEED_LIMIT);
		CmdSetTarget(target);
	)
}

void CCircuitUnit::Attack(const AIFloat3& pos, CEnemyInfo* enemy, bool isGround, bool isStatic, int timeout)
{
	TRY_UNIT(manager->GetCircuit(), this,
		if (circuitDef->IsAttrMelee()) {
			if (IsJumpReady()) {
				CmdJumpTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
				CmdFightTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
			} else {
				CmdMoveTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
			}
		} else {
			CmdMoveTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, timeout);
		}
		if (isGround) {  // los-cheat related
			CmdAttackGround(enemy->GetPos(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
		} else {
			unit->Attack(enemy->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
		}
		CmdFightTo(enemy->GetPos(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);  // los-cheat related
		CmdWantedSpeed(NO_SPEED_LIMIT);
		CmdSetTarget(target);
		if (circuitDef->IsAttrOnOff()) {
			unit->SetOn(isStatic == circuitDef->IsOnSlow());
		}
	)
}

void CCircuitUnit::Attack(const AIFloat3& position, CEnemyInfo* enemy, int tile, bool isGround, bool isStatic, int timeout)
{
	target = enemy;
	targetTile = tile;
	Attack(position, enemy, isGround, isStatic, timeout);
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
//		CmdPatrolTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY, timeout);
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

ICoreUnit::Id CCircuitUnit::GetUnitIdReclaim() const
{
	if (command->GetId() != CMD_RECLAIM) {
		return -1;
	}
	auto params = command->GetParams();
	return (params.size() == 1) ? params[0] : -1;
}

#ifdef DEBUG_VIS
void CCircuitUnit::Log()
{
	if (task != nullptr) {
		task->Log();
	}
	CCircuitAI* circuit = manager->GetCircuit();
	if (travelAct != nullptr) {
		travelAct->Log(circuit);
	}
	GetPos(circuit->GetLastFrame());
	circuit->LOG("unit: %lx | id: %i | %f, %f, %f | %s", this, id, position.x, position.y, position.z, circuitDef->GetDef()->GetName());
	auto commands = unit->GetCurrentCommands();
	for (springai::Command* c : commands) {
		circuit->LOG("command: %i | type: %i | id: %i", c->GetCommandId(), c->GetType(), c->GetId());
	}
	utils::free_clear(commands);
}
#endif

} // namespace circuit
