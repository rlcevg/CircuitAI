/*
 * BuilderManager.h
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_BUILDERMANAGER_H_
#define SRC_CIRCUIT_MODULE_BUILDERMANAGER_H_

#include "module/UnitModule.h"
#include "task/builder/BuilderTask.h"
#include "terrain/TerrainData.h"
#include "unit/CircuitUnit.h"

#include <map>
#include <set>
#include <vector>
#include <unordered_set>

namespace springai {
	class AIFloat3;
}

namespace circuit {

class IGridLink;
class CQueryCostMap;
class CBRepairTask;
class CBReclaimTask;
class CCombatTask;

struct SBuildChain;

class CBuilderManager: public IUnitModule {
public:
	friend class CBuilderScript;

	CBuilderManager(CCircuitAI* circuit);
	virtual ~CBuilderManager();

private:
	void ReadConfig();
	void Init();
public:
	void Release();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitIdle(CCircuitUnit* unit) override;
	virtual int UnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker) override;

	CCircuitDef* GetTerraDef() const { return terraDef; }

	float GetGoalExecTime() const { return goalExecTime; }
	unsigned int GetWorkerCount() const { return workers.size(); }
	void AddBuildPower(CCircuitUnit* unit);
	void DelBuildPower(CCircuitUnit* unit);
	float GetBuildPower() const { return buildPower; }
	bool CanEnqueueTask(const unsigned mod = 8) const { return buildTasksCount < workers.size() * mod; }
	const std::set<IBuilderTask*>& GetTasks(IBuilderTask::BuildType type) const;
	void ActivateTask(IBuilderTask* task);

	IBuilderTask* EnqueueTask(IBuilderTask::Priority priority,
							  CCircuitDef* buildDef,
							  const springai::AIFloat3& position,
							  IBuilderTask::BuildType type,
							  float cost,
							  float shake = SQUARE_SIZE * 32,  // Alter/randomize position by offset
							  bool isActive = true,  // Should task go to general queue or remain detached?
							  int timeout = ASSIGN_TIMEOUT);
	IBuilderTask* EnqueueTask(IBuilderTask::Priority priority,
							  CCircuitDef* buildDef,
							  const springai::AIFloat3& position,
							  IBuilderTask::BuildType type,
							  float shake = SQUARE_SIZE * 32,
							  bool isActive = true,
							  int timeout = ASSIGN_TIMEOUT);
	IBuilderTask* EnqueueDefence(IBuilderTask::Priority priority,
								 CCircuitDef* buildDef,
								 int pointId,
								 const springai::AIFloat3& position,
								 IBuilderTask::BuildType type,
								 float cost,
								 float shake = SQUARE_SIZE * 32,
								 bool isActive = true,
								 int timeout = ASSIGN_TIMEOUT);
	IBuilderTask* EnqueueSpot(IBuilderTask::Priority priority,
							  CCircuitDef* buildDef,
							  int spotId,
							  const springai::AIFloat3& position,
							  IBuilderTask::BuildType type,
							  bool isActive = true,
							  int timeout = ASSIGN_TIMEOUT);
	IBuilderTask* EnqueueFactory(IBuilderTask::Priority priority,
								 CCircuitDef* buildDef,
								 CCircuitDef* reprDef,
								 const springai::AIFloat3& position,
								 float shake = SQUARE_SIZE * 32,
								 bool isPlop = false,
								 bool isActive = true,
								 int timeout = ASSIGN_TIMEOUT);
	IBuilderTask* EnqueuePylon(IBuilderTask::Priority priority,
							   CCircuitDef* buildDef,
							   const springai::AIFloat3& position,
							   IGridLink* link,
							   float cost,
							   bool isActive = true,
							   int timeout = ASSIGN_TIMEOUT);
	IBuilderTask* EnqueueRepair(IBuilderTask::Priority priority,
								CCircuitUnit* target,
								int timeout = ASSIGN_TIMEOUT);
	IBuilderTask* EnqueueReclaim(IBuilderTask::Priority priority,
								 const springai::AIFloat3& position,
								 float cost,
								 int timeout,
								 float radius = .0f,
								 bool isMetal = true);
	IBuilderTask* EnqueueReclaim(IBuilderTask::Priority priority,
								 CCircuitUnit* target,
								 int timeout = ASSIGN_TIMEOUT);
	IBuilderTask* EnqueueResurrect(IBuilderTask::Priority priority,
								   const springai::AIFloat3& position,
								   float cost,
								   int timeout,
								   float radius = .0f);
	IBuilderTask* EnqueuePatrol(IBuilderTask::Priority priority,
								const springai::AIFloat3& position,
								float cost,
								int timeout);
	IBuilderTask* EnqueueTerraform(IBuilderTask::Priority priority,
								   CCircuitUnit* target,
								   const springai::AIFloat3& position = -RgtVector,
								   float cost = 1.0f,
								   bool isActive = true,
								   int timeout = ASSIGN_TIMEOUT);
	IBuilderTask* EnqueueGuard(IBuilderTask::Priority priority,
							   CCircuitUnit* target,
							   bool isInterrupt,
							   int timeout = ASSIGN_TIMEOUT);
	IUnitTask* EnqueueWait(int timeout);
	virtual CRetreatTask* EnqueueRetreat() override;
	CCombatTask* EnqueueCombat(float powerMod);

private:
	IBuilderTask* AddTask(IBuilderTask::Priority priority,
						  CCircuitDef* buildDef,
						  const springai::AIFloat3& position,
						  IBuilderTask::BuildType type,
						  SResource cost,
						  float shake,
						  bool isActive,
						  int timeout);
	virtual void DequeueTask(IUnitTask* task, bool done = false) override;

public:
	virtual void FallbackTask(CCircuitUnit* unit) override;

	void MarkUnfinishedUnit(CAllyUnit* target, IBuilderTask* task) {
		unfinishedUnits[target] = task;
	}
	void MarkRepairUnit(ICoreUnit::Id targetId, CBRepairTask* task) {
		repairUnits[targetId] = task;
	}
	void MarkReclaimUnit(CAllyUnit* target, CBReclaimTask* task) {
		reclaimUnits[target] = task;
	}

	bool IsBuilderInArea(CCircuitDef* buildDef, const springai::AIFloat3& position) const;  // Check if build-area has proper builder
	bool HasFreeAssists(CCircuitDef* conDef) const;
	SBuildChain* GetBuildChain(IBuilderTask::BuildType buildType, CCircuitDef* cdef) const;

	IBuilderTask* GetRepairTask(ICoreUnit::Id unitId) const;
	IBuilderTask* GetReclaimFeatureTask(const springai::AIFloat3& pos, float radius) const;
	IBuilderTask* GetResurrectTask(const springai::AIFloat3& pos, float radius) const;
	void RegisterReclaim(CAllyUnit* unit) { reclaimUnits[unit] = nullptr; }
	void UnregisterReclaim(CAllyUnit* unit) { reclaimUnits.erase(unit); }
	bool IsReclaimUnit(CAllyUnit* unit) const { return reclaimUnits.find(unit) != reclaimUnits.end(); }
	bool IsReclaimFeature(const springai::AIFloat3& pos, float radius) const {
		return GetReclaimFeatureTask(pos, radius) != nullptr;
	}
	bool IsResurrect(const springai::AIFloat3& pos, float radius) const {
		return GetResurrectTask(pos, radius) != nullptr;
	}

	void SetCanUpMex(CCircuitDef* cdef, bool value);
	bool CanUpMex(CCircuitDef* cdef) const;
	void SetCanUpGeo(CCircuitDef* cdef, bool value);
	bool CanUpGeo(CCircuitDef* cdef) const;

	void IncGuardCount() { ++guardCount; }
	void DecGuardCount() { --guardCount; }

private:
	virtual IUnitTask* DefaultMakeTask(CCircuitUnit* unit) override;
	IBuilderTask* MakeEnergizerTask(CCircuitUnit* unit, const CQueryCostMap* query);
	IBuilderTask* MakeCommPeaceTask(CCircuitUnit* unit, const CQueryCostMap* query, float sqMaxBaseRange);
	IBuilderTask* MakeCommDangerTask(CCircuitUnit* unit, const CQueryCostMap* query, float sqMaxBaseRange);
	IBuilderTask* MakeBuilderTask(CCircuitUnit* unit, const CQueryCostMap* query);
	IBuilderTask* CreateBuilderTask(const springai::AIFloat3& position, CCircuitUnit* unit);

	void AddBuildList(CCircuitUnit* unit, int hiddenDefs);
	void RemoveBuildList(CCircuitUnit* unit, int hiddenDefs);

	void Watchdog();
	void UpdateIdle();
	void UpdateBuild();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	EHandlers damagedHandler;
	EHandlers destroyedHandler;

	std::map<CAllyUnit*, IBuilderTask*> unfinishedUnits;
	std::map<ICoreUnit::Id, CBRepairTask*> repairUnits;
	std::map<CAllyUnit*, CBReclaimTask*> reclaimUnits;
	std::vector<std::set<IBuilderTask*>> buildTasks;  // UnitDef based tasks
	unsigned int assistCount;  // builders that can assist
	unsigned int guardCount;  // assist guards
	unsigned int buildTasksCount;
	float buildPower;
	std::vector<IUnitTask*> buildUpdates;  // owner
	unsigned int buildIterator;

	float goalExecTime;  // seconds
	std::set<CCircuitUnit*> workers;
	std::map<CCircuitUnit*, std::shared_ptr<IPathQuery>> costQueries;  // IPathQuery owner

	CCircuitDef* terraDef;
	std::unordered_map<IBuilderTask::BT, std::unordered_map<CCircuitDef*, SBuildChain*>> buildChains;  // owner
	struct SSuper {
		float minIncome;  // metal per second
		float maxTime;  // seconds
	} super;

	struct SWorkExt {
		bool canUpMex : 1;
		bool canUpGeo : 1;
	};
	std::unordered_map<CCircuitDef*, SWorkExt> workerDefs;

public:
	void UpdateAreaUsers();
private:
	std::unordered_set<terrain::SMobileType::Id> workerMobileTypes;
	std::map<terrain::SArea*, std::map<CCircuitDef*, int>> buildAreas;  // area <=> worker types

	virtual void Load(std::istream& is) override;
	virtual void Save(std::ostream& os) const override;

#ifdef DEBUG_VIS
public:
	void Log();
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_BUILDERMANAGER_H_
