/*
 * EconomyManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef ECONOMYMANAGER_H_
#define ECONOMYMANAGER_H_

#include "Module.h"

#include "AIFloat3.h"
#include <map>
#include <list>
#include <vector>
#include <unordered_set>
#include <functional>

namespace springai {
	class Resource;
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
	void Update();
	void WorkerWatchdog();
	CCircuitUnit* FindUnitToAssist(CCircuitUnit* unit);
	void PrepareFactory(CCircuitUnit* unit);
	void ExecuteFactory(CCircuitUnit* unit);
	void PrepareBuilder(CCircuitUnit* unit);
	void ExecuteBuilder(CCircuitUnit* unit);

	using Handlers = std::map<int, std::function<void (CCircuitUnit* unit)>>;
	Handlers finishedHandler;
	Handlers idleHandler;
	Handlers destroyedHandler;
	std::map<CCircuitUnit*, IConstructTask*> unfinishedUnits;
	std::map<IConstructTask*, std::list<CCircuitUnit*>> unfinishedTasks;
	float totalBuildpower;
	springai::Resource* metalRes;
	springai::Resource* energyRes;
	std::list<IConstructTask*> builderTasks;  // owner
	std::list<IConstructTask*> factoryTasks;  // owner

	struct WorkerInfo {
		CCircuitUnit* unit;
		springai::AIFloat3 pos;
		float qspeed;
	};
	std::unordered_set<CCircuitUnit*> workers;
	int cachedFrame;
	using WorkerTaskRelation = std::vector<std::vector<WorkerInfo*>>;
	WorkerTaskRelation wtRelation;
	WorkerTaskRelation& GetWorkerTaskRelations(CCircuitUnit* unit, WorkerInfo*& retInfo);

	struct BuilderInfo {
		int startFrame;
	};
	std::map<CCircuitUnit*, BuilderInfo> builderInfo;
};

} // namespace circuit

#endif // ECONOMYMANAGER_H_
