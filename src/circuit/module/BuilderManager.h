/*
 * BuilderManager.h
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_BUILDERMANAGER_H_
#define SRC_CIRCUIT_BUILDERMANAGER_H_

#include "module/UnitModule.h"
#include "task/BuilderTask.h"

#include "AIFloat3.h"

#include <map>
#include <set>
#include <vector>

namespace circuit {

class CBuilderManager: public IUnitModule {
public:
	CBuilderManager(CCircuitAI* circuit);
	virtual ~CBuilderManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	float GetBuilderPower();
	bool CanEnqueueTask();
	const std::set<CBuilderTask*>& GetTasks(CBuilderTask::BuildType type);
	CBuilderTask* EnqueueTask(CBuilderTask::Priority priority,
							  springai::UnitDef* buildDef,
							  const springai::AIFloat3& position,
							  CBuilderTask::BuildType type,
							  float cost,
							  int timeout = 0);
	CBuilderTask* EnqueueTask(CBuilderTask::Priority priority,
							  springai::UnitDef* buildDef,
							  const springai::AIFloat3& position,
							  CBuilderTask::BuildType type,
							  int timeout = 0);
	CBuilderTask* EnqueueTask(CBuilderTask::Priority priority,
							  const springai::AIFloat3& position,
							  CBuilderTask::BuildType type,
							  int timeout = 0);
private:
	inline void AddTask(CBuilderTask* task, CBuilderTask::BuildType type);
public:
	void DequeueTask(CBuilderTask* task);
	virtual void AssignTask(CCircuitUnit* unit);
	virtual void ExecuteTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task, CCircuitUnit* unit = nullptr);
	virtual void OnUnitDamaged(CCircuitUnit* unit);

private:
	void Init();
	void Watchdog();
	void Update();
	CCircuitUnit* FindUnitToAssist(CCircuitUnit* unit);

	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers2 damagedHandler;
	Handlers2 destroyedHandler;

	std::map<CCircuitUnit*, CBuilderTask*> unfinishedUnits;
	std::vector<std::set<CBuilderTask*>> builderTasks;  // owner
	int builderTasksCount;
	float builderPower;

	std::set<CCircuitUnit*> workers;

	// TODO: Move into CBuilderTask ?
	struct BuilderInfo {
		int startFrame;
	};
	std::map<CCircuitUnit*, BuilderInfo> builderInfos;  // Assistant's info
};

} // namespace circuit

#endif // SRC_CIRCUIT_BUILDERMANAGER_H_
