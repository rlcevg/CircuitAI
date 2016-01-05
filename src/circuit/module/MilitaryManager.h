/*
 * MilitaryManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_
#define SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_

#include "module/UnitModule.h"
#include "task/fighter/FighterTask.h"

#include <vector>
#include <set>

namespace circuit {

class CBDefenceTask;
class CDefenceMatrix;
class CRetreatTask;

class CMilitaryManager: public IUnitModule {
public:
	CMilitaryManager(CCircuitAI* circuit);
	virtual ~CMilitaryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitIdle(CCircuitUnit* unit) override;
	virtual int UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker) override;

	IFighterTask* EnqueueTask(IFighterTask::FightType type);
	CRetreatTask* EnqueueRetreat();
private:
	void DequeueTask(IFighterTask* task, bool done = false);

public:
	virtual void AssignTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task);
	virtual void DoneTask(IUnitTask* task);
	virtual void FallbackTask(CCircuitUnit* unit);

	void MakeDefence(const springai::AIFloat3& pos);
	void AbortDefence(CBDefenceTask* task);
	springai::AIFloat3 GetScoutPosition(CCircuitUnit* unit);

	bool IsNeedAA(CCircuitDef* cdef) const;
	bool IsNeedArty(CCircuitDef* cdef) const;

private:
	void ReadConfig();
	void Init();

	void UpdateIdle();
	void UpdateRetreat();
	void UpdateFight();

	void AddPower(CCircuitDef* cdef, const float scale = 1.0f);
	void DelPower(CCircuitDef* cdef) { AddPower(cdef, -1.0f); }

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	EHandlers damagedHandler;
	EHandlers destroyedHandler;

	std::set<IFighterTask*> fightTasks;  // owner
	std::set<IFighterTask*> fightUpdateTasks;
	std::set<IFighterTask*> fightDeleteTasks;
	unsigned int fightUpdateSlice;

	std::set<CRetreatTask*> retreatTasks;  // owner
	std::set<CRetreatTask*> retUpdateTasks;
	std::set<CRetreatTask*> retDeleteTasks;
	unsigned int retUpdateSlice;

	CDefenceMatrix* defence;

	std::set<CCircuitDef*> scoutDefs;
	std::vector<unsigned int> scoutPath;  // list of cluster ids
	unsigned int scoutIdx;

	float metalAA, ratioAA, maxPercAA, factorAA;
	float metalArty, ratioArty, maxPercArty, factorArty;
	float metalLand;
	float metalWater;
	float metalSum;

	// FIXME: DEBUG
	float curPowah;
	// FIXME: DEBUG
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_
