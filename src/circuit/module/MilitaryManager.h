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

class CCircuitDef;
class CBDefenceTask;

class CMilitaryManager: public IUnitModule {
public:
	CMilitaryManager(CCircuitAI* circuit);
	virtual ~CMilitaryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitIdle(CCircuitUnit* unit) override;
	virtual int UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker) override;

	IUnitTask* EnqueueTask(IFighterTask::FightType type);
private:
	void DequeueTask(IUnitTask* task, bool done = false);

public:
	virtual void AssignTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task);
	virtual void DoneTask(IUnitTask* task);
	virtual void FallbackTask(CCircuitUnit* unit);

	void MakeDefence(const springai::AIFloat3& pos);
	void AbortDefence(CBDefenceTask* task);
	int GetScoutIndex();

private:
	void Init();

	void UpdateIdle();
	void UpdateRetreat();
	void UpdateFight();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	EHandlers damagedHandler;
	EHandlers destroyedHandler;

	std::set<IUnitTask*> fighterTasks;  // owner
	std::set<IUnitTask*> updateTasks;
	std::set<IUnitTask*> deleteTasks;
	unsigned int updateSlice;

	std::set<CCircuitDef*> scouts;
	std::vector<int> scoutPath;  // list of cluster ids
	unsigned int curScoutIdx;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_
