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
class CKMeansCluster;

class CMilitaryManager: public IUnitModule {
public:
	CMilitaryManager(CCircuitAI* circuit);
	virtual ~CMilitaryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitIdle(CCircuitUnit* unit) override;
	virtual int UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker) override;

	const std::set<IFighterTask*>& GetTasks(IFighterTask::FightType type) const {
		return fightTasks[static_cast<IFighterTask::FT>(type)];
	}

	IFighterTask* EnqueueTask(IFighterTask::FightType type);
	IFighterTask* EnqueueGuard(CCircuitUnit* vip);
	CRetreatTask* EnqueueRetreat();
private:
	void DequeueTask(IFighterTask* task, bool done = false);

public:
	virtual IUnitTask* MakeTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task);
	virtual void DoneTask(IUnitTask* task);
	virtual void FallbackTask(CCircuitUnit* unit);

	void MakeDefence(const springai::AIFloat3& pos);
	void AbortDefence(CBDefenceTask* task);
	bool HasDefence(int cluster);
	springai::AIFloat3 GetScoutPosition(CCircuitUnit* unit);

	IFighterTask* AddDefendTask(int cluster);
	IFighterTask* DelDefendTask(const springai::AIFloat3& pos);
	IFighterTask* DelDefendTask(int cluster);
	IFighterTask* GetDefendTask(int cluster) const { return clusterInfos[cluster].defence; }

	bool IsNeedRole(CCircuitDef* cdef, CCircuitDef::RoleType type) const;
	bool IsNeedBigGun(CCircuitDef* cdef) const;

	void UpdateDefenceTasks();

private:
	void ReadConfig();
	void Init();

	void Watchdog();
	void UpdateIdle();
	void UpdateRetreat();
	void UpdateFight();

	void AddPower(CCircuitUnit* unit);
	void DelPower(CCircuitUnit* unit);

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	EHandlers damagedHandler;
	EHandlers destroyedHandler;

	std::vector<std::set<IFighterTask*>> fightTasks;  // owner
	std::set<IFighterTask*> fightUpdateTasks;
	std::set<IFighterTask*> fightDeleteTasks;
	unsigned int fightUpdateSlice;

	std::set<IUnitTask*> retreatTasks;  // owner
	std::set<IUnitTask*> retUpdateTasks;
	std::set<IUnitTask*> retDeleteTasks;
	unsigned int retUpdateSlice;

	CDefenceMatrix* defence;

	std::vector<unsigned int> scoutPath;  // list of cluster ids
	unsigned int scoutIdx;

	struct SRoleInfo {
		float metal;
		float ratio;
		float maxPerc;
		float factor;
		std::set<CCircuitUnit*> units;
		std::vector<CCircuitDef::RoleType> vs;
	};
	std::vector<SRoleInfo> roleInfos;

	std::set<CCircuitUnit*> army;
	float metalArmy;
	CKMeansCluster* enemyGroups;

	struct SClusterInfo {
		IFighterTask* defence;
	};
	std::vector<SClusterInfo> clusterInfos;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_
