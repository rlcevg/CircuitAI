/*
 * EconomyManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef ECONOMYMANAGER_H_
#define ECONOMYMANAGER_H_

#include "Module.h"
#include "BuilderTask.h"

#include "AIFloat3.h"
#include <map>
#include <list>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>

namespace springai {
	class Resource;
	class Economy;
}

namespace circuit {

class IConstructTask;

class CEconomyManager: public virtual IModule {
public:
	CEconomyManager(CCircuitAI* circuit);
	virtual ~CEconomyManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId);
	virtual int UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId);

private:
	void Init();
	void UpdateExpandTasks();
	void UpdateEnergyTasks();
	void UpdateBuilderTasks();
	void UpdateFactoryTasks();
	void WorkerWatchdog();
	CCircuitUnit* FindUnitToAssist(CCircuitUnit* unit);
	void PrepareFactory(CCircuitUnit* unit);
	void ExecuteFactory(CCircuitUnit* unit);
	void PrepareBuilder(CCircuitUnit* unit);
	void ExecuteBuilder(CCircuitUnit* unit);

	using Handlers1 = std::unordered_map<int, std::function<void (CCircuitUnit* unit)>>;
	using Handlers2 = std::unordered_map<int, std::function<void (CCircuitUnit* unit, CCircuitUnit* builder)>>;
	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers2 destroyedHandler;
	std::map<CCircuitUnit*, IConstructTask*> unfinishedUnits;
	std::map<IConstructTask*, std::list<CCircuitUnit*>> unfinishedTasks;
	springai::Resource* metalRes;
	springai::Resource* energyRes;
	springai::Economy* eco;
	float totalBuildpower;
	std::map<CBuilderTask::TaskType, std::list<IConstructTask*>> builderTasks;  // owner
	float builderPower;
	int builderTasksCount;
	std::list<IConstructTask*> factoryTasks;  // owner
	float factoryPower;

	std::unordered_set<CCircuitUnit*> workers;

	std::map<CCircuitUnit*, std::list<CCircuitUnit*>> factories;

	// TODO: Move into CBuilderTask ?
	struct BuilderInfo {
		int startFrame;
	};
	std::map<CCircuitUnit*, BuilderInfo> builderInfo;

	struct ClusterInfo {
		CCircuitUnit* factory;
		CCircuitUnit* pylon;
	};
	std::vector<ClusterInfo> clusterInfo;
	int solarCount;
	int fusionCount;
	float pylonRange;
//	float singuRange;

#ifdef DEBUG
	std::vector<springai::AIFloat3> panicList;
#endif
};

} // namespace circuit

#endif // ECONOMYMANAGER_H_
