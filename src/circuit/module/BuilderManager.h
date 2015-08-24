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
#include "static/TerrainData.h"

#include <map>
#include <set>
#include <vector>
#include <unordered_set>

namespace springai {
	class AIFloat3;
}

namespace circuit {

class CCircuitDef;
class CEnergyLink;

class CBuilderManager: public IUnitModule {
public:
	CBuilderManager(CCircuitAI* circuit);
	virtual ~CBuilderManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	CCircuitDef* GetTerraDef() const { return terraDef; }

	int GetWorkerCount() const { return workers.size(); }
	float GetBuilderPower() const { return builderPower; }
	bool CanEnqueueTask() const { return builderTasksCount < workers.size() * 2; }
	const std::set<IBuilderTask*>& GetTasks(IBuilderTask::BuildType type);
	void ActivateTask(IBuilderTask* task);
	IBuilderTask* EnqueueTask(IBuilderTask::Priority priority,
							  CCircuitDef* buildDef,
							  const springai::AIFloat3& position,
							  IBuilderTask::BuildType type,
							  float cost,
							  bool isShake = true,  // Should position be randomly shifted?
							  bool isActive = true,  // Should task go to general queue or remain detached?
							  int timeout = 0);
	IBuilderTask* EnqueueTask(IBuilderTask::Priority priority,
							  CCircuitDef* buildDef,
							  const springai::AIFloat3& position,
							  IBuilderTask::BuildType type,
							  bool isShake = true,
							  bool isActive = true,
							  int timeout = 0);
	IBuilderTask* EnqueuePylon(IBuilderTask::Priority priority,
							   CCircuitDef* buildDef,
							   const springai::AIFloat3& position,
							   CEnergyLink* link,
							   float cost,
							   bool isActive = true,
							   int timeout = 0);
	IBuilderTask* EnqueuePatrol(IBuilderTask::Priority priority,
								const springai::AIFloat3& position,
								float cost,
								int timeout);
	IBuilderTask* EnqueueReclaim(IBuilderTask::Priority priority,
								 const springai::AIFloat3& position,
								 float cost,
								 int timeout,
								 float radius = .0f,
								 bool isMetal = true);
	IBuilderTask* EnqueueRepair(IBuilderTask::Priority priority,
								CCircuitUnit* target,
								int timeout = 0);
	IBuilderTask* EnqueueTerraform(IBuilderTask::Priority priority,
								   CCircuitUnit* target,
								   float cost = 1.0f,
								   int timeout = 0);
private:
	IBuilderTask* AddTask(IBuilderTask::Priority priority,
						  CCircuitDef* buildDef,
						  const springai::AIFloat3& position,
						  IBuilderTask::BuildType type,
						  float cost,
						  bool isShake,
						  bool isActive,
						  int timeout);
	void DequeueTask(IBuilderTask* task, bool done = false);

public:
	bool IsBuilderInArea(CCircuitDef* buildDef, const springai::AIFloat3& position);  // Check if build-area has proper builder

	virtual void AssignTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task);
	virtual void DoneTask(IUnitTask* task);
	virtual void SpecialCleanUp(CCircuitUnit* unit);
	virtual void SpecialProcess(CCircuitUnit* unit);
	virtual void FallbackTask(CCircuitUnit* unit);

private:
	void Init();
	void Watchdog();

	void AddBuildList(CCircuitUnit* unit);
	void RemoveBuildList(CCircuitUnit* unit);

	void UpdateIdle();
	void UpdateRetreat();
	void UpdateBuild();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers2 damagedHandler;
	Handlers2 destroyedHandler;

	std::map<CCircuitUnit*, IBuilderTask*> unfinishedUnits;
	std::vector<std::set<IBuilderTask*>> builderTasks;  // owner
	int builderTasksCount;
	float builderPower;
	std::set<IBuilderTask*> updateTasks;  // temporary tasks holder to keep updating every task
	std::set<IBuilderTask*> deleteTasks;
	unsigned int updateSlice;

	std::set<CCircuitUnit*> workers;
	std::set<CCircuitUnit*> assistants;  // workers with temporary task

	CCircuitDef* terraDef;

public:
	void UpdateAreaUsers();
private:
	std::unordered_set<STerrainMapMobileType::Id> workerMobileTypes;
	std::unordered_set<CCircuitDef*> workerDefs;
	std::map<STerrainMapArea*, std::map<CCircuitDef*, int>> buildAreas;  // area <=> worker types
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_BUILDERMANAGER_H_
