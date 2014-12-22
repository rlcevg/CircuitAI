/*
 * BuilderManager.h
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_BUILDERMANAGER_H_
#define SRC_CIRCUIT_BUILDERMANAGER_H_

#include "Module.h"
#include "BuilderTask.h"

#include "AIFloat3.h"

#include <list>
#include <map>
#include <set>
#include <vector>

namespace circuit {

class CBuilderManager: public virtual IModule {
public:
	CBuilderManager(CCircuitAI* circuit);
	virtual ~CBuilderManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	CBuilderTask* EnqueueTask(CBuilderTask::Priority priority,
							  springai::UnitDef* buildDef,
							  const springai::AIFloat3& position,
							  CBuilderTask::TaskType type,
							  float cost,
							  int timeout = 0);
	CBuilderTask* EnqueueTask(CBuilderTask::Priority priority,
							  springai::UnitDef* buildDef,
							  const springai::AIFloat3& position,
							  CBuilderTask::TaskType type,
							  int timeout = 0);
	void DequeueTask(CBuilderTask* task);
	float GetBuilderPower();
	bool CanEnqueueTask();
	const std::list<CBuilderTask*>& GetTasks(CBuilderTask::TaskType type);

private:
	void Init();
	void Watchdog();
	void AssignTask(CCircuitUnit* unit);
//	void ResignTask(CCircuitUnit* unit);
	void ExecuteTask(CCircuitUnit* unit);
	CCircuitUnit* FindUnitToAssist(CCircuitUnit* unit);

	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers2 damagedHandler;
	Handlers2 destroyedHandler;

	std::map<CCircuitUnit*, CBuilderTask*> unfinishedUnits;
	std::vector<std::list<CBuilderTask*>> builderTasks;  // owner
	int builderTasksCount;
	float builderPower;

	std::set<CCircuitUnit*> workers;

	// TODO: Move into CBuilderTask ?
	struct BuilderInfo {
		int startFrame;
	};
	std::map<CCircuitUnit*, BuilderInfo> builderInfo;  // Assistant's info
};

} // namespace circuit

#endif // SRC_CIRCUIT_BUILDERMANAGER_H_
