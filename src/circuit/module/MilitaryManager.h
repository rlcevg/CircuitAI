/*
 * MilitaryManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_
#define SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_

#include "module/UnitModule.h"
#include "setup/DefenceData.h"
#include "task/fighter/FighterTask.h"
#include "unit/CircuitDef.h"
#include "util/AvailList.h"

#include <vector>
#include <set>

namespace circuit {

class IMainJob;
class CBDefenceTask;
class CFGuardTask;

namespace TaskF {
	struct SFightTask {
		IFighterTask::FightType type;
		IFighterTask::FightType check;
		IFighterTask::FightType promote;
		float power;
		CCircuitUnit* vip;
	};

	static inline SFightTask Common(IFighterTask::FightType type)
	{
		SFightTask ti;
		ti.type = type;
		return ti;
	}
	static inline SFightTask Guard(CCircuitUnit* vip)
	{
		SFightTask ti;
		ti.type = IFighterTask::FightType::GUARD;
		ti.vip = vip;
		return ti;
	}
	static inline SFightTask Defend(IFighterTask::FightType promote, float power)
	{
		SFightTask ti;
		ti.type = IFighterTask::FightType::DEFEND;
		ti.check = IFighterTask::FightType::_SIZE_;  // NONE
		ti.promote = promote;
		ti.power = power;
		return ti;
	}
	static inline SFightTask Defend(IFighterTask::FightType check, IFighterTask::FightType promote, float power)
	{
		SFightTask ti;
		ti.type = IFighterTask::FightType::DEFEND;
		ti.check = check;
		ti.promote = promote;
		ti.power = power;
		return ti;
	}
} // namespace TaskF

class CMilitaryManager: public IUnitModule {
public:
	friend class CMilitaryScript;

	using BuildVector = std::vector<std::pair<CCircuitDef*, int>>;  // cdef: frame
	using SuperInfos = std::vector<std::pair<CCircuitDef*, float>>;  // cdef: weight
	struct SSideInfo {
		std::vector<CCircuitDef*> landDefenders;
		std::vector<CCircuitDef*> waterDefenders;
		BuildVector baseDefence;

		std::vector<CCircuitDef*> wallDefs;  // land and water
		std::vector<CCircuitDef*> chokeDefs;  // land and water
		CCircuitDef* defaultPorc;

		SuperInfos superInfos;
	};

	CMilitaryManager(CCircuitAI* circuit);
	virtual ~CMilitaryManager();

private:
	void ReadConfig();
	void InitEconomyScores(const std::vector<CCircuitDef*>&& builders);
	void Init();

public:
	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitIdle(CCircuitUnit* unit) override;
	virtual int UnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker) override;

	const std::set<IFighterTask*>& GetTasks(IFighterTask::FightType type) const {
		return fightTasks[static_cast<IFighterTask::FT>(type)];
	}

	IFighterTask* Enqueue(const TaskF::SFightTask& ti);
	virtual CRetreatTask* EnqueueRetreat() override;
private:
	virtual void DequeueTask(IUnitTask* task, bool done = false) override;

public:
	void MarkGuardUnit(CCircuitUnit* vip, CFGuardTask* task) {
		guardTasks[vip] = task;
	}

	void MakeDefence(const springai::AIFloat3& pos);
	void MakeDefence(int cluster);
	void MakeDefence(int cluster, const springai::AIFloat3& pos);
	void DefaultMakeDefence(int cluster, const springai::AIFloat3& pos);
	void MarkPorc(CCircuitUnit* unit, int defPointId);
	void UnmarkPorc(CCircuitUnit* unit);
	void AbortDefence(const CBDefenceTask* task, int defPointId);
	bool HasDefence(int cluster);
	void ProcessHubDefence(CBDefenceTask* task);
	springai::AIFloat3 GetScoutPosition(CCircuitUnit* unit);
	void ClearScoutPosition(IUnitTask* task);
	void FillFrontPos(CCircuitUnit* unit, F3Vec& outPositions);
	void FillAttackSafePos(CCircuitUnit* unit, F3Vec& outPositions);
	void FillStaticSafePos(CCircuitUnit* unit, F3Vec& outPositions);
	void FillSafePos(CCircuitUnit* unit, F3Vec& outPositions);
	CCircuitUnit* GetClosestLeader(IFighterTask::FightType type, const springai::AIFloat3& position);

	IFighterTask* GetGuardTask(CCircuitUnit* unit) const;

	const std::set<CCircuitUnit*>& GetRoleUnits(CCircuitDef::RoleT type) const {
		return roleInfos[type].units;
	}
	void AddResponse(CCircuitUnit* unit);
	void DelResponse(CCircuitUnit* unit);
	float GetArmyCost() const { return armyCost; }
	float RoleProbability(const CCircuitDef* cdef) const;
	bool IsNeedBigGun(const CCircuitDef* cdef) const;
	springai::AIFloat3 GetBigGunPos(CCircuitDef* bigDef) const;
	void DiceBigGun();
	float ClampMobileCostRatio() const;
	void UpdateDefenceTasks();
	void UpdateDefence();
	void MakeBaseDefence(const springai::AIFloat3& pos);

	void AddSensorDefs(const std::set<CCircuitDef*>& buildDefs);  // add available sensor defs
	void RemoveSensorDefs(const std::set<CCircuitDef*>& buildDefs);

	const SSideInfo& GetSideInfo() const;
	const std::vector<SSideInfo>& GetSideInfos() const { return sideInfos; }

	CCircuitDef* GetBigGunDef() const { return bigGunDef; }
	CCircuitDef* GetDefaultPorc() const { return GetSideInfo().defaultPorc; }
	CCircuitDef* GetLowSonar(const CCircuitUnit* builder = nullptr) const;

	void SetBaseDefRange(float range) { defence->SetBaseRange(range); }
	float GetBaseDefRange() const { return defence->GetBaseRange(); }
	float GetCommDefRadBegin() const { return defence->GetCommRadBegin(); }
	float GetCommDefRad(float baseDist) const { return defence->GetCommRad(baseDist); }
	unsigned int GetGuardTaskNum() const { return defence->GetGuardTaskNum(); }
	unsigned int GetGuardsNum() const { return defence->GetGuardsNum(); }
	int GetGuardFrame() const { return defence->GetGuardFrame(); }

	void AddPointOfInterest(CEnemyInfo* enemy) { PointOfInterest(enemy, +3, -1); }
	void DelPointOfInterest(CEnemyInfo* enemy) { PointOfInterest(enemy, -3, +1); }

	// TODO: Create CMilitaryManager::CTargetManager and move all FindTarget variations there.
	//       CMilitaryManager must be responsible for target selection.
	bool IsCombatTargetExists(CCircuitUnit* unit, const springai::AIFloat3& pos, float powerMod);  // logic must be similar to CCombatTask::FindTarget

private:
	virtual IUnitTask* DefaultMakeTask(CCircuitUnit* unit) override;

	void Watchdog();

	void AddArmyCost(CCircuitUnit* unit);
	void DelArmyCost(CCircuitUnit* unit);
	void PointOfInterest(CEnemyInfo* enemy, int start, int step);
	CDefenceData::SDefPoint* FindClosestDefPoint(const springai::AIFloat3& pos);
	CDefenceData::SDefPoint* FindClosestDefPoint(int cluster, const springai::AIFloat3& pos,
			std::function<bool (const CDefenceData::SDefPoint&)> predicate = nullptr);

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	EHandlers damagedHandler;
	EHandlers destroyedHandler;

	std::vector<std::set<IFighterTask*>> fightTasks;

	CDefenceData* defence;
	unsigned int defenceIdx;
	std::map<CCircuitUnit*, int> porcToPoint;  // unit: defPointId

	struct SScoutPoint {
		int spotNum;  // last used spot number in cluster
		int scouted;  // times this point was scouted
		int score;
		int enemyNum;
		IUnitTask* task;
	};
	std::vector<SScoutPoint> scoutPoints;  // list of clusters, index = custerId
	std::map<IUnitTask*, int> scoutTasks;  // task: clusterId
	bool isEnemyFound;

	struct SRoleInfo {
		float cost;
		float maxPerc;
		float factor;
		std::set<CCircuitUnit*> units;
		struct SVsInfo {
			SVsInfo(CCircuitDef::RoleT t, float r, float i) : role(t), ratio(r), importance(i) {}
			CCircuitDef::RoleT role;
			float ratio;
			float importance;
		};
		std::vector<SVsInfo> vs;
	};
	std::vector<SRoleInfo> roleInfos;

	std::set<CCircuitUnit*> stockpilers;
	std::set<CCircuitUnit*> army;
	float armyCost;

	std::map<CCircuitUnit*, CFGuardTask*> guardTasks;

	struct SRaidQuota {
		float min;
		float avg;
	} raid;
	unsigned int maxScouts;
	float minAttackers;
	struct SThreatQuota {
		float min;
		float len;
	} attackMod, defenceMod;

	unsigned int preventCount;
	float amountFactor;
	CCircuitDef* bigGunDef;

	std::vector<SSideInfo> sideInfos;

	struct SSensorExt {
		float radius;
	};
	CAvailList<SSensorExt> radarDefs, sonarDefs;

	std::shared_ptr<IMainJob> defend;
	std::vector<std::pair<springai::AIFloat3, BuildVector>> buildDefence;  // pos: defences
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_
