/*
 * CircuitUnit.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_
#define SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_

#include "util/ActionList.h"

#include "Unit.h"

namespace springai {
	class Weapon;
}

namespace circuit {

#define CMD_RETREAT_ZONE			10001
#define CMD_SETHAVEN				CMD_RETREAT_ZONE
#define CMD_ORBIT					13923
#define CMD_ORBIT_DRAW				13924
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
#define CMD_JUMP					38521
#define CMD_AIR_STRAFE				39381
#define CMD_TERRAFORM_INTERNAL		39801

class CCircuitDef;
class IUnitTask;
class IUnitManager;
struct STerrainMapArea;

class CCircuitUnit: public CActionList {
public:
	using Id = int;

	CCircuitUnit(const CCircuitUnit& that) = delete;
	CCircuitUnit& operator=(const CCircuitUnit&) = delete;
	CCircuitUnit(springai::Unit* unit, CCircuitDef* cdef);
	virtual ~CCircuitUnit();

	Id GetId() const { return id; }
	springai::Unit* GetUnit() const { return unit; }
	CCircuitDef* GetCircuitDef() const { return circuitDef; }

	void SetTask(IUnitTask* task);
	IUnitTask* GetTask() const { return task; }
	int GetTaskFrame() const { return taskFrame; }

	void SetManager(IUnitManager* mgr) { manager = mgr; }
	IUnitManager* GetManager() const { return manager; }

	void SetArea(STerrainMapArea* area) { this->area = area; }
	STerrainMapArea* GetArea() const { return area; }

	const springai::AIFloat3& GetPos(int frame);

	bool IsMoveFailed(int frame);

	void ForceExecute() { isForceExecute = true; }
	bool IsForceExecute();

	bool HasDGun();
	springai::Weapon* GetWeapon() const { return weapon; }
	springai::Weapon* GetDGun() const { return dgun; }
	springai::Weapon* GetShield() const { return shield; }
	void ManualFire(springai::Unit* enemy, int timeOut);
	bool IsDisarmed(int frame);
	bool IsWeaponReady(int frame);
	bool IsDGunReady(int frame);
//	bool IsShieldCharged(float percent);
	float GetDPS();

	void Guard(CCircuitUnit* target, int timeout);

	void Morph();
	void StopMorph();
	void Upgrade();
	void StopUpgrade();
	bool IsMorphing() const { return isMorphing; }

	bool operator==(const CCircuitUnit& rhs) { return id == rhs.id; }
	bool operator!=(const CCircuitUnit& rhs) { return id != rhs.id; }

private:
	Id id;
	springai::Unit* unit;  // owner
	CCircuitDef* circuitDef;  // TODO: Replace with CCircuitDef::Id?
	IUnitTask* task;
	int taskFrame;
	IUnitManager* manager;
	STerrainMapArea* area;  // = nullptr if a unit flies

	int posFrame;
	springai::AIFloat3 position;

	int moveFails;
	int failFrame;
	bool isForceExecute;  // TODO: Replace by CExecuteAction?

	springai::Weapon* weapon;  // main weapon
	springai::Weapon* dgun;
	springai::Weapon* shield;

	bool isDisarmed;
	int disarmFrame;

	bool isWeaponReady;
	int ammoFrame;

	bool isMorphing;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_CIRCUITUNIT_H_
