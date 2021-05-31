/*
 * CircuitUnit.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_
#define SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_

#include "unit/ally/AllyUnit.h"
#include "unit/CircuitDef.h"
#include "util/ActionList.h"
#include "util/Defines.h"

namespace springai {
	class Command;
	class Weapon;
}

namespace circuit {

#define TRY_UNIT(c, u, x)	try { x } catch (const std::exception& e) { c->Garbage(u, e.what()); }

#define UNIT_CMD_OPTION				0

#define CMD_ATTACK_GROUND			20
#define CMD_RETREAT_ZONE			10001
#define CMD_ORBIT					13923
#define CMD_ORBIT_DRAW				13924
#define CMD_CLOAK_SHIELD			31101
#define CMD_RAW_MOVE				31109
#define CMD_MORPH_UPGRADE_INTERNAL	31207
#define CMD_UPGRADE_STOP			31208
#define CMD_MORPH					31210
#define CMD_MORPH_STOP				32210
#define CMD_FIND_PAD				33411
#define CMD_PRIORITY				34220
#define CMD_MISC_PRIORITY			34221
#define CMD_RETREAT					34223
#define CMD_UNIT_SET_TARGET			34923
#define CMD_UNIT_CANCEL_TARGET		34924
#define CMD_ONECLICK_WEAPON			35000
#define CMD_WANT_CLOAK				37382
#define CMD_DONT_FIRE_AT_RADAR		38372
#define CMD_JUMP					38521
#define CMD_WANTED_SPEED			38825
#define CMD_AIR_STRAFE				39381
#define CMD_TERRAFORM_INTERNAL		39801

// FIXME: BA
#define CMD_AUTOMEX				31143
#define CMD_BAR_PRIORITY		34571
#define CMD_LAND_AT_AIRBASE		35430
// FIXME: BA

class CEnemyInfo;
class IUnitManager;
class CDGunAction;
class ITravelAction;
struct STerrainMapArea;

class CCircuitUnit: public CAllyUnit, public CActionList {
public:
	friend class CInitScript;

	CCircuitUnit(const CCircuitUnit& that) = delete;
	CCircuitUnit& operator=(const CCircuitUnit&) = delete;
	CCircuitUnit(Id unitId, springai::Unit* unit, CCircuitDef* cdef);
	virtual ~CCircuitUnit();

	void SetTask(IUnitTask* task);
	void SetTaskFrame(int frame) { taskFrame = frame; }
	int GetTaskFrame() const { return taskFrame; }

	void SetManager(IUnitManager* mgr) { manager = mgr; }
	IUnitManager* GetManager() const { return manager; }

	void SetArea(STerrainMapArea* area) { this->area = area; }
	STerrainMapArea* GetArea() const { return area; }

	void ClearAct();
	void PushDGunAct(CDGunAction* action);
	CDGunAction* GetDGunAct() const { return dgunAct; }
	void PushTravelAct(ITravelAction* action);
	ITravelAction* GetTravelAct() const { return travelAct; }

	bool IsMoveFailed(int frame);
	bool IsStuck() const { return isStuck; }

	void ForceUpdate(int frame);
	bool IsForceUpdate(int frame);

	void SetIsDead() { isDead = true; }
	bool IsDead() const { return isDead; }

//	void SetDamagedFrame(int frame) { damagedFrame = frame; }
//	int GetDamagedFrame() const { return damagedFrame; }

	bool HasDGun() const { return dgun != nullptr; }
	bool HasWeapon() const { return weapon != nullptr; }
	bool HasShield() const { return shield != nullptr; }
	void ManualFire(CEnemyInfo* target, int timeout);
	bool IsDisarmed(int frame);
	bool IsWeaponReady(int frame);
	bool IsDGunReady(int frame, float energy);
	bool IsShieldCharged(float percent);
	bool IsJumpReady();
	bool IsJumping();
	float GetDamage();
	float GetShieldPower();
	float GetBuildSpeed();
	float GetWorkerTime();
	float GetDGunRange();
	float GetHealthPercent();

	void CmdRemove(std::vector<float>&& params, short options = 0);
	void CmdMoveTo(const springai::AIFloat3& pos, short options = 0, int timeout = INT_MAX);
	void CmdJumpTo(const springai::AIFloat3& pos, short options = 0, int timeout = INT_MAX);
	void CmdAttackGround(const springai::AIFloat3& pos, short options = 0, int timeout = INT_MAX);
	void CmdWantedSpeed(float speed = NO_SPEED_LIMIT);
	void CmdSetTarget(CEnemyInfo* enemy);
	void CmdCloak(bool state);
	void CmdFireAtRadar(bool state);
	void CmdFindPad(int timeout = INT_MAX);
	void CmdManualFire(short options = 0, int timeout = INT_MAX);
	void CmdPriority(float value);
	void CmdMiscPriority(float value);
	void CmdAirStrafe(float value);
	void CmdBARPriority(float value);
	void CmdTerraform(std::vector<float>&& params);
	void CmdSelfD(bool state);
	bool IsInSelfD() const { return isSelfD; }
	void CmdWait(bool state);
	void RemoveWait();
	bool IsWaiting() const;
	void CmdRepair(CAllyUnit* target, short options = 0, int timeout = INT_MAX);
	void CmdBuild(CCircuitDef* buildDef, const springai::AIFloat3& buildPos, int facing, short options = 0, int timeout = INT_MAX);

	void Attack(CEnemyInfo* enemy, bool isGround, int timeout);
	void Attack(const springai::AIFloat3& position, CEnemyInfo* enemy, bool isGround, bool isStatic, int timeout);
	void Attack(const springai::AIFloat3& position, CEnemyInfo* enemy, int tile, bool isGround, bool isStatic, int timeout);
	void Guard(CCircuitUnit* target, int timeout);
	void Gather(const springai::AIFloat3& groupPos, int timeout);

	void Morph();
	void StopMorph();
	bool IsUpgradable();
	void Upgrade();
	void StopUpgrade();
	bool IsMorphing() const { return isMorphing; }

	bool IsInExecute() const { return isInExecute; }

	Id GetUnitIdReclaim() const;

	void ClearTarget() { target = nullptr; }
	CEnemyInfo* GetTarget() const { return target; }
	int GetTargetTile() const { return targetTile; }

	void AddAttribute(CCircuitDef::AttrType type) { attr |= CCircuitDef::GetMask(static_cast<CCircuitDef::AttrT>(type)); }
	bool IsAttrAny(CCircuitDef::AttrM value) const { return (attr & value) != 0; }
	bool IsAttrBase() const { return attr & CCircuitDef::AttrMask::BASE; }

private:
	// NOTE: taskFrame assigned on task change and OnUnitIdle to workaround idle spam.
	//       Proper fix: do not issue any commands OnUnitIdle, delay them until next frame?
	int taskFrame;
	IUnitManager* manager;
	STerrainMapArea* area;  // = nullptr if a unit flies

	CDGunAction* dgunAct;
	ITravelAction* travelAct;

	int moveFails;
	int failFrame;
//	int damagedFrame;
	int execFrame;  // TODO: Replace by CExecuteAction?
	int disarmFrame;
	int ammoFrame;

	float priority;

	// ---- Bit fields ---- BEGIN
	bool isDead : 1;
	bool isStuck : 1;
	bool isDisarmed : 1;
	bool isWeaponReady : 1;
	bool isMorphing : 1;
	bool isSelfD : 1;
	bool isInExecute : 1;
	// ---- Bit fields ---- END

	springai::Command* command;  // current top command
	springai::Weapon* dgun;
	springai::Weapon* weapon;  // main weapon
	springai::Weapon* shield;

	CEnemyInfo* target;
	int targetTile;

	CCircuitDef::AttrM attr;

#ifdef DEBUG_VIS
public:
	void Log();
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_
