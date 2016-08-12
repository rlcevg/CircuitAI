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
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"

#include <vector>
#include <set>

namespace circuit {

class CBDefenceTask;
class CDefenceMatrix;
class CRetreatTask;

class CMilitaryManager: public IUnitModule {
public:
	struct SEnemyGroup {
		SEnemyGroup(const springai::AIFloat3& p) : pos(p), cost(0.f), threat(0.f) {}
		std::vector<CCircuitUnit::Id> units;
		springai::AIFloat3 pos;
		std::array<float, static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_)> roleCosts{{0.f}};
		float cost;
		float threat;
	};

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

	float GetEnemyCost(CCircuitDef::RoleType type) const {
		return enemyCosts[static_cast<CCircuitDef::RoleT>(type)];
	}
	void AddEnemyCost(const CEnemyUnit* e);
	void DelEnemyCost(const CEnemyUnit* e);
	const std::vector<SEnemyGroup>& GetEnemyGroups() const { return enemyGroups; }
	void UpdateEnemyGroups() { KMeansIteration(); }

	float GetArmyCost() const { return armyCost; }
	float RoleProbability(const CCircuitDef* cdef) const;
	bool IsNeedBigGun(const CCircuitDef* cdef) const;

	void UpdateDefenceTasks();

	unsigned int GetMaxScouts() const { return maxScouts; }
	unsigned int GetMaxRaiders() const { return maxRaiders; }

	const std::vector<CCircuitDef*>& GetLandDefenders() const { return landDefenders; }
	const std::vector<CCircuitDef*>& GetWaterDefenders() const { return waterDefenders; }
	const std::vector<std::pair<CCircuitDef*, int>>& GetBaseDefence() const { return baseDefence; }
	CCircuitDef* GetBigGunDef() const { return bigGunDef; }
	CCircuitDef* GetDefaultPorc() const { return defaultPorc; }

private:
	void ReadConfig();
	void Init();

	void Watchdog();
	void UpdateIdle();
	void UpdateFight();

	void AddPower(CCircuitUnit* unit);
	void DelPower(CCircuitUnit* unit);
	void KMeansIteration();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	EHandlers damagedHandler;
	EHandlers destroyedHandler;

	std::vector<std::set<IFighterTask*>> fightTasks;
	std::vector<IUnitTask*> fightUpdates;  // owner
	unsigned int fightIterator;

	CDefenceMatrix* defence;

	std::vector<unsigned int> scoutPath;  // list of cluster ids
	unsigned int scoutIdx;

	struct SRoleInfo {
		float cost;
		float maxPerc;
		float factor;
		std::set<CCircuitUnit*> units;
		struct SVsInfo {
			SVsInfo(CCircuitDef::RoleType t, float r, float i) : role(t), ratio(r), importance(i) {}
			CCircuitDef::RoleType role;
			float ratio;
			float importance;
		};
		std::vector<SVsInfo> vs;
	};
	std::vector<SRoleInfo> roleInfos;

	std::set<CCircuitUnit*> army;
	float armyCost;

	std::array<float, static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_)> enemyCosts{{0.f}};
	std::vector<SEnemyGroup> enemyGroups;

	struct SClusterInfo {
		IFighterTask* defence;
	};
	std::vector<SClusterInfo> clusterInfos;

	unsigned int maxRaiders;
	unsigned int maxScouts;
	unsigned int maxGuards;

	std::vector<CCircuitDef*> defenderDefs;
	std::vector<CCircuitDef*> landDefenders;
	std::vector<CCircuitDef*> waterDefenders;
	std::vector<std::pair<CCircuitDef*, int>> baseDefence;
	CCircuitDef* radarDef;
	CCircuitDef* sonarDef;
	CCircuitDef* bigGunDef;
	CCircuitDef* defaultPorc;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_
