/*
 * CircuitUnit.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_
#define SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_

#include "unit/AllyUnit.h"
#include "util/ActionList.h"

namespace springai {
	class Weapon;
}

namespace circuit {

#define TRY_UNIT(c, u, x)	try { x } catch (const std::exception& e) { c->Garbage(u, e.what()); }

#define CMD_ATTACK_GROUND			20
#define CMD_RETREAT_ZONE			10001
#define CMD_SETHAVEN				CMD_RETREAT_ZONE
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

class CCircuitDef;
class CEnemyUnit;
class IUnitManager;
struct STerrainMapArea;

class CCircuitUnit: public CAllyUnit, public CActionList {
public:
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

	bool IsMoveFailed(int frame);

	void ForceExecute() { isForceExecute = true; }
	bool IsForceExecute();

	void Dead() { isDead = true; }
	bool IsDead() const { return isDead; }

//	void SetDamagedFrame(int frame) { damagedFrame = frame; }
//	int GetDamagedFrame() const { return damagedFrame; }

	bool HasDGun() const { return dgun != nullptr; }
	bool HasWeapon() const { return weapon != nullptr; }
	bool HasShield() const { return shield != nullptr; }
	void ManualFire(CEnemyUnit* target, int timeOut);
	bool IsDisarmed(int frame);
	bool IsWeaponReady(int frame);
	bool IsDGunReady(int frame);
	bool IsShieldCharged(float percent);
	bool IsJumpReady();
	bool IsJumping();
	float GetDamage();
	float GetShieldPower();
	float GetBuildSpeed();
	float GetDGunRange();
	float GetHealthPercent();

	void Attack(CEnemyUnit* target, int timeout);
	void Attack(const springai::AIFloat3& position, int timeout);
	void Attack(const springai::AIFloat3& position, CEnemyUnit* target, int timeout);
	void Guard(CCircuitUnit* target, int timeout);
	void Gather(const springai::AIFloat3& groupPos, int timeout);

	void Morph();
	void StopMorph();
	bool IsUpgradable();
	void Upgrade();
	void StopUpgrade();
	bool IsMorphing() const { return isMorphing; }

private:
	// NOTE: taskFrame assigned on task change and OnUnitIdle to workaround idle spam.
	//       Proper fix: do not issue any commands OnUnitIdle, delay them until next frame?
	int taskFrame;
	IUnitManager* manager;
	STerrainMapArea* area;  // = nullptr if a unit flies

//	int damagedFrame;
	int moveFails;
	int failFrame;
	bool isForceExecute;  // TODO: Replace by CExecuteAction?
	bool isDead;

	springai::Weapon* dgun;
	springai::Weapon* weapon;  // main weapon
	springai::Weapon* shield;

	bool isDisarmed;
	int disarmFrame;

	bool isWeaponReady;
	int ammoFrame;

	bool isMorphing;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_
