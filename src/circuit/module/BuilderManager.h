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

class CEnergyLink;
class CRetreatTask;
class CBRepairTask;
class CBReclaimTask;

struct SBuildChain;

class CBuilderManager: public IUnitModule {
public:
	CBuilderManager(CCircuitAI* circuit);
	virtual ~CBuilderManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitIdle(CCircuitUnit* unit) override;
	virtual int UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker) override;

	CCircuitDef* GetTerraDef() const { return terraDef; }

	unsigned int GetWorkerCount() const { return workers.size(); }
	void AddBuildPower(CCircuitUnit* unit);
	void DelBuildPower(CCircuitUnit* unit);
	float GetBuildPower() const { return buildPower; }
	bool CanEnqueueTask() const { return buildTasksCount < workers.size() * 8; }
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
	IBuilderTask* EnqueueFactory(IBuilderTask::Priority priority,
								 CCircuitDef* buildDef,
								 const springai::AIFloat3& position,
								 float shake = SQUARE_SIZE * 32,
								 bool isPlop = false,
								 bool isActive = true,
								 int timeout = ASSIGN_TIMEOUT);
	IBuilderTask* EnqueuePylon(IBuilderTask::Priority priority,
							   CCircuitDef* buildDef,
							   const springai::AIFloat3& position,
							   CEnergyLink* link,
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
							   int timeout = ASSIGN_TIMEOUT);
	IUnitTask* EnqueueWait(int timeout);
	CRetreatTask* EnqueueRetreat();

private:
	IBuilderTask* AddTask(IBuilderTask::Priority priority,
						  CCircuitDef* buildDef,
						  const springai::AIFloat3& position,
						  IBuilderTask::BuildType type,
						  float cost,
						  float shake,
						  bool isActive,
						  int timeout);
	void DequeueTask(IBuilderTask* task, bool done = false);

public:
	bool IsBuilderInArea(CCircuitDef* buildDef, const springai::AIFloat3& position);  // Check if build-area has proper builder

	virtual IUnitTask* MakeTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task);
	virtual void DoneTask(IUnitTask* task);
	virtual void FallbackTask(CCircuitUnit* unit);

	SBuildChain* GetBuildChain(IBuilderTask::BuildType buildType, CCircuitDef* cdef);

	bool IsReclaimed(CCircuitUnit* unit) const { return reclaimedUnits.find(unit) != reclaimedUnits.end(); }

private:
	void ReadConfig();
	void Init();
public:
	void Release();

private:
	IBuilderTask* MakeCommTask(CCircuitUnit* unit);
	IBuilderTask* MakeBuilderTask(CCircuitUnit* unit);
	IBuilderTask* CreateBuilderTask(const springai::AIFloat3& position, CCircuitUnit* unit);

	void AddBuildList(CCircuitUnit* unit);
	void RemoveBuildList(CCircuitUnit* unit);

	void Watchdog();
	void UpdateIdle();
	void UpdateBuild();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	EHandlers damagedHandler;
	EHandlers destroyedHandler;

	std::map<CCircuitUnit*, IBuilderTask*> unfinishedUnits;
	std::map<CCircuitUnit::Id, CBRepairTask*> repairedUnits;
	std::map<CCircuitUnit*, CBReclaimTask*> reclaimedUnits;
	std::vector<std::set<IBuilderTask*>> buildTasks;  // UnitDef based tasks
	unsigned int buildTasksCount;
	float buildPower;
	std::vector<IUnitTask*> buildUpdates;  // owner
	unsigned int buildIterator;

	std::set<CCircuitUnit*> workers;

	CCircuitDef* terraDef;
	std::unordered_map<IBuilderTask::BT, std::unordered_map<CCircuitDef*, SBuildChain*>> buildChains;  // owner
	struct SSuper {
		float minIncome;  // metal per second
		float maxTime;  // seconds
	} super;

public:
	void UpdateAreaUsers();
private:
	std::unordered_set<STerrainMapMobileType::Id> workerMobileTypes;
	std::unordered_set<CCircuitDef*> workerDefs;
	std::map<STerrainMapArea*, std::map<CCircuitDef*, int>> buildAreas;  // area <=> worker types

	virtual void Load(std::istream& is);
	virtual void Save(std::ostream& os) const;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_BUILDERMANAGER_H_
